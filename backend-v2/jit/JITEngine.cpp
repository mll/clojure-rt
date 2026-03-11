#include "JITEngine.h"
#include "bridge/Exceptions.h"
#include <llvm/ExecutionEngine/Orc/DebugObjectManagerPlugin.h>
#include <llvm/ExecutionEngine/Orc/EPCDebugObjectRegistrar.h>
#include <llvm/ExecutionEngine/Orc/ObjectTransformLayer.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/MemoryBuffer.h>

namespace rt {

JITEngine::JITEngine(ThreadsafeCompilerState &state, size_t numThreads)
    : pool(std::max(numThreads / 4, size_t(1)), Priority::Low),
      threadsafeState(state) {

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto JITExp = llvm::orc::LLJITBuilder()
                    .setObjectLinkingLayerCreator(
                        [&](llvm::orc::ExecutionSession &ES,
                            const llvm::Triple &TT) {
                          auto ObjLinkingLayer =
                              std::make_unique<llvm::orc::ObjectLinkingLayer>(ES);

                          // Add debug info support
                          auto Registrar = llvm::orc::createJITLoaderGDBRegistrar(ES);
                          if (Registrar) {
                            ObjLinkingLayer->addPlugin(
                                std::make_unique<llvm::orc::DebugObjectManagerPlugin>(
                                    ES, std::move(*Registrar)));
                          }

                          return ObjLinkingLayer;
                        })
                    .create();
  if (!JITExp)
    throw std::runtime_error("Failed to init LLJIT");
  jit = std::move(*JITExp);

  // Set up object transform to capture compiled objects
  jit->getObjTransformLayer().setTransform(
      [this](std::unique_ptr<llvm::MemoryBuffer> Obj)
          -> llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> {
        std::lock_guard<std::mutex> lock(this->engineMutex);
        std::string id = Obj->getBufferIdentifier().str();
        // Store a copy of the buffer content
        this->capturedObjectBuffers[id] = llvm::MemoryBuffer::getMemBufferCopy(
            Obj->getBuffer(), id);
        return std::move(Obj);
      });

  // Expose symbols from the current process to the JIT.
  // This is required on Linux to find runtime functions.
  auto &DL = jit->getDataLayout();
  jit->getMainJITDylib().addGenerator(
      cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          DL.getGlobalPrefix())));
}

std::future<llvm::orc::ExecutorAddr>
JITEngine::compileAST(const Node &AST, const std::string &moduleName,
                      llvm::OptimizationLevel level, bool printModule) {
  // Wrap logic in a task for the pool
  return pool.enqueue(
      [this, AST, moduleName, printModule]() -> llvm::orc::ExecutorAddr {
        auto codeGenerator = CodeGen(moduleName, threadsafeState);

        string fName;
        try {
          fName = codeGenerator.codegenTopLevel(AST);
        } catch (...) {
          auto result = std::move(codeGenerator).release();
          for (auto c : result.constants) {
            release(c);
          }
          throw;
        }

        auto result = std::move(codeGenerator).release();
        auto context = std::move(result.context);
        auto module = std::move(result.module);
        auto constants = std::move(result.constants);

        // 4. Optimize WITHOUT LOCK
        // if (level != llvm::OptimizationLevel::O0) {
        //   this->optimize(*mod, Level);
        // }

        // --- TUTAJ WYPISUJEMY IR ---
        if (module && printModule) {
          llvm::outs() << "\n=== Generated LLVM IR for: '" << moduleName
                       << "' ===\n";
          module->print(llvm::outs(),
                        nullptr); // Wypisuje IR do standardowego wyjścia LLVM
          llvm::outs() << "=============================================\n";
          llvm::outs().flush(); // Wymuszamy opróżnienie bufora, żeby tekst nie
                                // zniknął przy błędzie
        }
        // ---------------------------

        llvm::orc::ThreadSafeModule TSM(std::move(module), *context);

        {
          std::lock_guard<std::mutex> lock(engineMutex);
          auto RT = jit->getMainJITDylib().createResourceTracker();

          // ORC takes ownership of TSM and the Context inside it
          if (auto Err = jit->addIRModule(RT, std::move(TSM)))
            throwInternalInconsistencyException("JIT Link Error");

          functionTrackers[moduleName] = RT;
          moduleConstants[moduleName] = std::move(constants);
        }

        // 6. Return executable address
        auto Sym = jit->lookup(fName);
        if (!Sym)
          throwInternalInconsistencyException("Symbol Lookup Failed: " + fName);

        // Register for stack trace resolution
        {
          std::lock_guard<std::mutex> lock(this->engineMutex);
          
          // Try to find the captured object buffer. ORC might append suffixes.
          auto it = this->capturedObjectBuffers.find(moduleName);
          if (it == this->capturedObjectBuffers.end()) {
             // Try common ORC suffixes if exact match fails
             it = this->capturedObjectBuffers.find(moduleName + "-jitted-objectbuffer");
          }
          if (it == this->capturedObjectBuffers.end()) {
             // Fallback: search for any key containing moduleName
             for (auto searchIt = this->capturedObjectBuffers.begin(); searchIt != this->capturedObjectBuffers.end(); ++searchIt) {
                 if (searchIt->first.find(moduleName) != std::string::npos) {
                     it = searchIt;
                     break;
                 }
             }
          }

          if (it != this->capturedObjectBuffers.end()) {
            registerJitFunction_C(Sym->getValue(), 4096, fName.c_str(),
                                  it->second->getBufferStart(),
                                  it->second->getBufferSize());
            this->capturedObjectBuffers.erase(it);
          } else {
            registerJitFunction_C(Sym->getValue(), 4096, fName.c_str(), nullptr,
                                  0);
          }
        }

        return *Sym;
      });
}

void JITEngine::invalidate(const std::string &name) {
  std::lock_guard<std::mutex> lock(engineMutex);

  if (auto itConst = moduleConstants.find(name);
      itConst != moduleConstants.end()) {
    for (auto val : itConst->second) {
      release(val);
    }
    moduleConstants.erase(itConst);
  }

  if (auto it = functionTrackers.find(name); it != functionTrackers.end()) {
    cantFail(it->second->remove());
    functionTrackers.erase(it);
  }
}

JITEngine::~JITEngine() {
  std::vector<std::string> names;
  {
    std::lock_guard<std::mutex> lock(engineMutex);
    for (auto const &[name, _] : functionTrackers) {
      names.push_back(name);
    }
  }
  for (auto const &name : names) {
    invalidate(name);
  }
}

} // namespace rt
