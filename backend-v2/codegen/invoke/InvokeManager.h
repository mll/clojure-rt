#ifndef INVOKE_MANAGER_H
#define INVOKE_MANAGER_H

#include "../../state/ThreadsafeCompilerState.h"
#include "../LLVMTypes.h"
#include "../TypedValue.h"
#include "../ValueEncoder.h"
#include "bytecode.pb.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <unordered_map>

using namespace clojure::rt::protobuf::bytecode;
using IntrinsicCall = std::function<llvm::Value *(llvm::IRBuilder<> &,
                                                  std::vector<llvm::Value *>)>;

#include "../../tools/EdnParser.h"

namespace rt {
class InvokeManager {
private:
  llvm::IRBuilder<> &builder;
  llvm::Module &theModule;
  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  std::unordered_map<std::string, IntrinsicCall> intrinsics;

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
};

} // namespace rt

#endif // INVOKE_MANAGER_H
