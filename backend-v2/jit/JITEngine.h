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
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
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
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <functional>
#include <unordered_map>
#include <cstdint>

#include "../runtime/JITSafety.h"


namespace rt {

class JITEngine {
public:

  struct SafetySection {
    JITEngine &engine;
    SafetySection(JITEngine &e);
    ~SafetySection();
  };

private:
  std::unique_ptr<llvm::orc::LLJIT> jit;
  std::unique_ptr<llvm::TargetMachine> TM;
  ThreadPool pool;
  std::mutex engineMutex;
  std::map<std::string, llvm::orc::ResourceTrackerSP> functionTrackers;
  std::map<std::string, std::vector<RTValue>> moduleConstants;
  std::map<std::string, std::unique_ptr<llvm::MemoryBuffer>> capturedObjectBuffers;

  // Epoch-based Reclamation (EBR)
  struct ZombieTracker {
    llvm::orc::ResourceTrackerSP tracker;
    uint64_t epoch;
    std::vector<RTValue> constants;
  };
  struct LimboTracker {
    llvm::orc::ResourceTrackerSP tracker;
    std::vector<RTValue> constants;
  };
  std::atomic<uint64_t> globalEpoch{1};
  std::vector<ZombieTracker> zombieTrackers;
  std::map<std::string, std::vector<LimboTracker>> limboTrackers;
  std::map<std::thread::id, std::atomic<uint64_t>*> activeThreads;
  mutable std::mutex zombieMutex;

  ThreadsafeCompilerState &threadsafeState;

  // Active compilation tracking
  std::unordered_map<std::string, std::shared_future<llvm::orc::ExecutorAddr>>
      activeCompilations;
  std::mutex compilationMutex;
  
  std::unique_ptr<llvm::MemoryBuffer> runtimeBitcodeBuffer;
  
  void optimize(llvm::Module &M, llvm::OptimizationLevel Level, const std::string &entryPoint);
  void registerRuntimeSymbols();

  // Private helper for common JIT logic
  std::shared_future<llvm::orc::ExecutorAddr> compileGeneric(
      std::function<std::string(CodeGen&)> codegenFunc,
      const std::string &moduleName,
      llvm::OptimizationLevel Level,
      bool printModule,
      bool reuseIfExists = false);

public:
  JITEngine(ThreadsafeCompilerState &state,
            size_t numThreads = std::thread::hardware_concurrency());
  ~JITEngine();
  /**
   * The primary entry point.
   * Returns a future that resolves to the function address once compiled.
   */
  std::shared_future<llvm::orc::ExecutorAddr> compileAST(const Node &AST,
                                                  const std::string &moduleName,
                                                  llvm::OptimizationLevel Level,
                                                  bool printModule = false);

  /**
   * Compiles a specialized bridge for an instance call.
   */
  std::shared_future<llvm::orc::ExecutorAddr> compileInstanceCallBridge(
      const std::string &methodName,
      const ObjectTypeSet &instanceType,
      const std::vector<ObjectTypeSet> &argTypes,
      void* callSiteId,
      llvm::OptimizationLevel Level,
      bool printModule = false);

  void invalidate(const std::string &name);
  
  /**
   * EBR management
   */
  void enterSafeSection();
  void leaveSafeSection();
  void commit(const std::string &name);
  void sweep();

  // Testing helpers
  size_t getLimboCount() const;
  size_t getZombieCount() const;

  void registerThread(rt_jt_epoch_t *epochPtr);
  void unregisterThread();

  uint64_t getGlobalEpoch() const { return globalEpoch.load(std::memory_order_relaxed); }

private:
  static std::atomic<bool> instanceExists;
};

} // namespace rt

#endif // JIT_ENGINE_H
