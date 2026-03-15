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
                            llvm::Module &m,
                            ValueEncoder &v, LLVMTypes &t,
                            VariableBindings<TypedValue> &vb, InvokeManager &i);

  void initFunction(llvm::Function *F);
  
  void dynamicMemoryGuidance(const MemoryManagementGuidance &guidance);

  void dynamicRetain(TypedValue &target);
  TypedValue dynamicRelease(TypedValue &target);
  void dynamicIsReusable(TypedValue &target);

  // Exception safety / Resource management
  void pushResource(TypedValue val);
  void popResource();
  llvm::BasicBlock* getLandingPad(size_t skipCount = 0);
  bool hasPushedResources() const { return !activeResources.empty(); }
  void clear();

private:
  llvm::LLVMContext &context;
  llvm::IRBuilder<> &builder;
  llvm::Module &theModule;
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

  void ensureExceptionInfrastructure(llvm::Function *F);
};

} // namespace rt

#endif // VALUE_ENCODER_H
