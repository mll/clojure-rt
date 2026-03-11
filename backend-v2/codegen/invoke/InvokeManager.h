#ifndef INVOKE_MANAGER_H
#define INVOKE_MANAGER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <functional>
#include <vector>
#include <string>

#include "../TypedValue.h"
#include "../ValueEncoder.h"
#include "../../types/ObjectTypeSet.h"

namespace rt {

class ThreadsafeCompilerState;
class IntrinsicDescription;

using IntrinsicCall = std::function<llvm::Value *(llvm::IRBuilder<> &,
                                                  std::vector<llvm::Value *>)>;
using TypeIntrinsicCall =
    std::function<ObjectTypeSet(const std::vector<ObjectTypeSet> &)>;

class InvokeManager {
private:
  llvm::IRBuilder<> &builder;
  llvm::Module &theModule;
  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  std::unordered_map<std::string, IntrinsicCall> intrinsics;
  std::unordered_map<std::string, TypeIntrinsicCall> typeIntrinsics;

public:
  explicit InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m, ValueEncoder &v,
                         LLVMTypes &t, ThreadsafeCompilerState &s);

  TypedValue invokeRuntime(const std::string &fname,
                           const ObjectTypeSet *retValType,
                           const std::vector<ObjectTypeSet> &argTypes,
                           const std::vector<TypedValue> &args,
                           const bool isVariadic = false);

  TypedValue generateIntrinsic(const IntrinsicDescription &id,
                               const std::vector<TypedValue> &args);

  ObjectTypeSet foldIntrinsic(const IntrinsicDescription &id,
                              const std::vector<ObjectTypeSet> &args);
};

} // namespace rt

#endif // INVOKE_MANAGER_H
