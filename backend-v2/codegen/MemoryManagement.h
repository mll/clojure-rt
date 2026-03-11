#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include "DynamicConstructor.h"
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
private:
  llvm::LLVMContext &context;
  llvm::IRBuilder<> &builder;
  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  VariableBindings<TypedValue> &variableBindingStack;
  InvokeManager &invoke;
  DynamicConstructor &dynamicConstructor;

public:
  explicit MemoryManagement(llvm::LLVMContext &context, llvm::IRBuilder<> &b,
                            ValueEncoder &v, LLVMTypes &t,
                            VariableBindings<TypedValue> &vb, InvokeManager &i,
                            DynamicConstructor &d);

  void dynamicMemoryGuidance(const MemoryManagementGuidance &guidance);

  void dynamicRetain(TypedValue &target);
  TypedValue dynamicRelease(TypedValue &target);
  void dynamicIsReusable(TypedValue &target);
};

} // namespace rt

#endif // VALUE_ENCODER_H
