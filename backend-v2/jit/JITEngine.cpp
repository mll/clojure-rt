#include "JITEngine.h"
#include "bridge/Exceptions.h"

namespace rt {

JITEngine::JITEngine(ThreadsafeCompilerState &state, size_t numThreads)
    : pool(std::max(numThreads / 4, size_t(1)), Priority::Low),
      threadsafeState(state) {

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto JITExp = llvm::orc::LLJITBuilder().create();
  if (!JITExp)
    throw std::runtime_error("Failed to init LLJIT");
  jit = std::move(*JITExp);

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

        // Register for stack trace resolution (approximate size)
        registerJitFunction_C(Sym->getValue(), 4096, fName.c_str());

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
