#include "JITEngine.h"

namespace rt {

  JITEngine::JITEngine(ThreadsafeCompilerState &state, size_t numThreads)    
    : pool(std::max(numThreads / 4, size_t(1)), Priority::Low), threadsafeState(state) {
    
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    
    auto JITExp = llvm::orc::LLJITBuilder().create();
    if (!JITExp) throw std::runtime_error("Failed to init LLJIT");
    jit = std::move(*JITExp);
  }

  std::future<llvm::orc::ExecutorAddr>
  JITEngine::compileAST(const Node &AST, const std::string &moduleName,
                        llvm::OptimizationLevel level) {
    // Wrap logic in a task for the pool
    return pool.enqueue([this, AST, level, moduleName]() -> llvm::orc::ExecutorAddr {           
      auto codeGenerator = CodeGen(moduleName, threadsafeState);
      auto fName = codeGenerator.codegenTopLevel(AST);
      
      auto [context, module] = std::move(codeGenerator).release();
      
      // 4. Optimize WITHOUT LOCK
      // if (level != llvm::OptimizationLevel::O0) {
      //   this->optimize(*mod, Level);
      // }

      // --- TUTAJ WYPISUJEMY IR ---
      if (module) {
        llvm::outs() << "\n--- Generated LLVM IR for: " << moduleName << " ---\n";
        module->print(llvm::outs(), nullptr); // Wypisuje IR do standardowego wyjścia LLVM
        llvm::outs() << "------------------------------------------\n";
        llvm::outs().flush(); // Wymuszamy opróżnienie bufora, żeby tekst nie zniknął przy błędzie
      }
      // ---------------------------
      
      llvm::orc::ThreadSafeModule TSM(std::move(module), *context);
      
      {
        std::lock_guard<std::mutex> lock(engineMutex);
        auto RT = jit->getMainJITDylib().createResourceTracker();
        
        // ORC takes ownership of TSM and the Context inside it
        if (auto Err = jit->addIRModule(RT, std::move(TSM)))
          throw std::runtime_error("JIT Link Error");
        
        functionTrackers[moduleName] = RT;
      }
      
      // 6. Return executable address
      auto Sym = jit->lookup(fName);
      if (!Sym) throw std::runtime_error("Symbol Lookup Failed");
      return *Sym;
    });
  }

  void JITEngine::invalidate(const std::string& name) {
    std::lock_guard<std::mutex> lock(engineMutex);
    if (auto it = functionTrackers.find(name); it != functionTrackers.end()) {
      cantFail(it->second->remove());
      functionTrackers.erase(it);
    }
  }  
  

} // namespace rt
  
