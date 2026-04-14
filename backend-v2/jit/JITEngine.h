#ifndef JIT_ENGINE_H
#define JIT_ENGINE_H

// LLVM Core & IR
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

// LLVM JIT (ORC v2)
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"

// LLVM Optimization (New Pass Manager)
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"

// Support & Targets
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

// Standard Library
#include "../codegen/CodeGen.h"
#include "../state/ThreadsafeCompilerState.h"
#include "../tools/ThreadPool.h"
#include "bytecode.pb.h"
#include <algorithm>
#include <bitset>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace rt {

struct JITResult {
  llvm::orc::ExecutorAddr address;
  std::string optimizedIR;
  std::string symbolName;
};

class JITEngine {
  struct Finaliser {
    ~Finaliser();
  };
  /* A trick to run code after all the variables were destroyed */
  std::mutex engineMutex;
  std::mutex compilationMutex;
  std::unique_ptr<llvm::orc::LLJIT> jit;
  std::unique_ptr<llvm::TargetMachine> TM;
  Finaliser finaliser;
  ThreadPool compilationPool;
  ThreadPool executionPool;

  struct CapturedObject {
    std::unique_ptr<llvm::MemoryBuffer> buffer;
    std::unordered_set<std::string> symbols;
  };
  std::vector<CapturedObject> capturedObjectBuffers;

  // Active compilation tracking
  std::unordered_map<std::string, std::shared_future<JITResult>>
      activeCompilations;

  std::unique_ptr<llvm::MemoryBuffer> runtimeBitcodeBuffer;

  void optimize(llvm::Module &M, const std::string &entryPoint);
  void registerRuntimeSymbols();

  // Private helper for common JIT logic
  std::shared_future<JITResult>
  compileGeneric(std::function<std::string(CodeGen &)> codegenFunc,
                 const std::string &moduleName, bool reuseIfExists = false);

public:
  ThreadsafeCompilerState threadsafeState;
  llvm::OptimizationLevel optLevel;
  bool printIR;
  JITEngine(
      llvm::OptimizationLevel defaultOptLevel = llvm::OptimizationLevel::O0,
      bool defaultPrintIR = false,
      size_t numThreads = std::thread::hardware_concurrency());
  ~JITEngine();
  /**
   * The primary entry point.
   * Returns a future that resolves to the function address once compiled.
   */
  std::shared_future<JITResult> compileAST(const Node &AST,
                                           const std::string &moduleName);

  /**
   * Compiles a specialized bridge for an instance call.
   */
  std::shared_future<JITResult> compileInstanceCallBridge(
      const std::string &methodName, const ObjectTypeSet &instanceType,
      const std::vector<ObjectTypeSet> &argTypes, void *callSiteId);

  void removeModule(llvm::orc::ResourceTrackerSP &tracker);
  void retireModule(const std::string &moduleName);

  struct ModuleTracker {
    llvm::orc::ResourceTrackerSP tracker;
    std::vector<RTValue> constants;
    std::vector<void *> icSlotAddresses;
  };

private:
  static CapturedObject captureObject(const llvm::MemoryBuffer &Obj);
  static std::atomic<bool> instanceExists;
  std::map<std::string, ModuleTracker *> moduleTrackers;
  void retireModule(ModuleTracker *tracker);
};

} // namespace rt

#endif // JIT_ENGINE_H
