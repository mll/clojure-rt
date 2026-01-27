#ifndef JIT_ENGINE_H
#define JIT_ENGINE_H

// LLVM Core & IR
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

// LLVM JIT (ORC v2)
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"

// LLVM Optimization (New Pass Manager)
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"

// Support & Targets
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

// Standard Library
#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <future>
#include <algorithm>
#include "../bytecode.pb.h"
#include "../state/ThreadsafeState.h"
#include "../codegen/CodeGen.h"
#include "../tools/ThreadPool.h"

namespace rt {

  class JITEngine {
    std::unique_ptr<llvm::orc::LLJIT> jit;
    ThreadPool pool;
    std::mutex engineMutex;
    std::map<std::string, llvm::orc::ResourceTrackerSP> functionTrackers;
    ThreadsafeCompilerState& threadsafeState;
  public:
    JITEngine(ThreadsafeCompilerState &state,
              size_t numThreads = std::thread::hardware_concurrency());
    /**
     * The primary entry point. 
     * Returns a future that resolves to the function address once compiled.
     */
    std::future<llvm::orc::ExecutorAddr>
    compileAST(const Node &AST, const std::string &moduleName,
               llvm::OptimizationLevel Level,
               bool printModule = false);
    
    void invalidate(const std::string& name);
  };
  
} // namespace rt

#endif // JIT_ENGINE_H
