#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include "LLVMTypes.h"
#include "TypedValue.h"
#include "ValueEncoder.h"
#include "VariableBindings.h"
#include "bytecode.pb.h"
#include "invoke/InvokeManager.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

using namespace clojure::rt::protobuf::bytecode;

namespace rt {

class MemoryManagement {
public:
  explicit MemoryManagement(llvm::LLVMContext &context, llvm::IRBuilder<> &b,
                            llvm::Module &m, ValueEncoder &v, LLVMTypes &t,
                            VariableBindings<TypedValue> &vb, InvokeManager &i);

  void initFunction(llvm::Function *F);

  void dynamicMemoryGuidance(const MemoryManagementGuidance &guidance);

  void dynamicRetain(TypedValue &target);
  TypedValue dynamicRelease(TypedValue &target);
  void dynamicIsReusable(TypedValue &target);

  // Exception safety / Resource management
  void pushResource(TypedValue val);
  void popResource();
  void popResourceWithoutRelease();
  llvm::BasicBlock *getLandingPad(
      size_t skipCount = 0,
      const std::vector<TypedValue> &extraCleanup = {});
  bool hasPushedResources() const { return !activeResources.empty(); }
  bool hasActiveGuidance() const {
    return activeUnwindGuidance != nullptr && !activeUnwindGuidance->empty();
  }
  void clear();

  struct FunctionState {
    llvm::Value *exceptionSlot;
    llvm::BasicBlock *terminalResumeBB;
    std::vector<llvm::BasicBlock *> cleanupStack;
    std::vector<TypedValue> activeResources;
    size_t totalPushedResources;
    size_t resourcesWithCleanup;
    std::map<size_t, llvm::BasicBlock *> lpadCache;
    const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
        *activeUnwindGuidance;
  };

  void pushState(llvm::Function *F);
  void popState();

  const google::protobuf::RepeatedPtrField<MemoryManagementGuidance> *
  getActiveUnwindGuidance() const {
    return activeUnwindGuidance;
  }
  void setActiveUnwindGuidance(
      const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
          *guidance) {
    activeUnwindGuidance = guidance;
    // Clearing cache because guidance might change between nodes for the same
    // skipCount
    lpadCache.clear();
  }
  void clearActiveUnwindGuidance() {
    activeUnwindGuidance = nullptr;
    lpadCache.clear();
  }

  struct UnwindGuidanceGuard {
    MemoryManagement &mm;
    const google::protobuf::RepeatedPtrField<MemoryManagementGuidance> *prev;
    UnwindGuidanceGuard(
        MemoryManagement &mm,
        const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
            *current)
        : mm(mm), prev(mm.getActiveUnwindGuidance()) {
      mm.setActiveUnwindGuidance(current);
    }
    ~UnwindGuidanceGuard() { mm.setActiveUnwindGuidance(prev); }
  };

private:
  llvm::LLVMContext &context;
  llvm::IRBuilder<> &builder;

  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  VariableBindings<TypedValue> &variableBindingStack;
  InvokeManager &invoke;

  // Landing pad / Exception state
  llvm::Value *exceptionSlot = nullptr;
  llvm::BasicBlock *terminalResumeBB = nullptr;
  std::vector<llvm::BasicBlock *> cleanupStack;
  std::vector<TypedValue> activeResources;
  size_t totalPushedResources = 0;
  size_t resourcesWithCleanup = 0; // Index into activeResources
  std::map<size_t, llvm::BasicBlock *> lpadCache;

  const google::protobuf::RepeatedPtrField<MemoryManagementGuidance>
      *activeUnwindGuidance = nullptr;
  std::vector<FunctionState> stateStack;
  void *jitEnginePtr = nullptr;

  void ensureExceptionInfrastructure(llvm::Function *F);
  void popResourceInternal(bool release);
};

} // namespace rt

#endif // VALUE_ENCODER_H
