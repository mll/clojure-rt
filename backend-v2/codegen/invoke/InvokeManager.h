#ifndef INVOKE_MANAGER_H
#define INVOKE_MANAGER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include "../TypedValue.h"
#include "../ValueEncoder.h"
#include "../LLVMTypes.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"


using namespace clojure::rt::protobuf::bytecode;
using IntrinsicCall = std::function<llvm::Value *(llvm::IRBuilder<> &,
                                               std::vector<llvm::Value *>)>;

namespace rt {
  
  class InvokeManager {
  private:
    llvm::IRBuilder<> &builder;
    llvm::Module &theModule;
    ValueEncoder &valueEncoder;
    LLVMTypes &types;
    std::unordered_map<std::string, IntrinsicCall> intrinsics;
    ThreadsafeCompilerState &state;
  public:
    explicit InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                           ValueEncoder &v, LLVMTypes &t, ThreadsafeCompilerState &s);

    TypedValue invokeRuntime(const std::string &fname,
                             const ObjectTypeSet *retValType,
                             const std::vector<ObjectTypeSet> &argTypes,
                             const std::vector<TypedValue> &args,
                             const bool isVariadic = false);

    TypedValue generateIntrinsic(RTValue intrinsicDescription,
                                 const std::vector<TypedValue> &args);
    bool checkIntrinsicArgs(PersistentArrayMap *description,
                            const std::vector<TypedValue> &args);
    ObjectTypeSet returnValueType(PersistentArrayMap *description);    
  };

} // namespace rt

#endif // INVOKE_MANAGER_H
