#ifndef INVOKE_MANAGER_H
#define INVOKE_MANAGER_H

#include "bytecode.pb.h"
#include <functional>
#include <gmp.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../types/ObjectTypeSet.h"
#include "../LLVMTypes.h"
#include "../TypedValue.h"
#include "../ValueEncoder.h"

namespace rt {

class CodeGen;
class ThreadsafeCompilerState;
class IntrinsicDescription;
class CleanupChainGuard;

using IntrinsicCall = std::function<llvm::Value *(llvm::IRBuilder<> &,
                                                  std::vector<llvm::Value *>)>;
using TypeIntrinsicCall =
    std::function<ObjectTypeSet(const std::vector<ObjectTypeSet> &)>;
using GenericIntrinsicCall = std::function<llvm::Value *(
    llvm::IRBuilder<> &, const std::vector<TypedValue> &, CleanupChainGuard *)>;

class InvokeManager {
  friend void registerMathIntrinsics(InvokeManager &mgr);
  friend void registerCmpIntrinsics(InvokeManager &mgr);

public:
  llvm::IRBuilder<> &builder;
  llvm::Module &theModule;
  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  CodeGen &codeGen;
  ThreadsafeCompilerState &compilerState;

private:
  std::unordered_map<std::string, IntrinsicCall> intrinsics;
  std::unordered_map<std::string, GenericIntrinsicCall> genericIntrinsics;
  std::unordered_map<std::string, TypeIntrinsicCall> typeIntrinsics;
  size_t icCounter = 0;

  TypedValue generateDeterminedInstanceCall(
      const std::string &methodName, TypedValue instance,
      const std::vector<TypedValue> &args, CleanupChainGuard *guard = nullptr,
      const clojure::rt::protobuf::bytecode::Node *node = nullptr);

  TypedValue generateDynamicInstanceCall(
      const std::string &methodName, TypedValue instance,
      const std::vector<TypedValue> &args, CleanupChainGuard *guard = nullptr,
      const clojure::rt::protobuf::bytecode::Node *node = nullptr);

  // Folding Helpers
  mpz_ptr getZ(const ObjectTypeSet &t);
  mpq_ptr getQ(const ObjectTypeSet &t);
  double getD(const ObjectTypeSet &t);
  ObjectTypeSet createZ(mpz_ptr val);
  ObjectTypeSet createQ(mpq_ptr val);
  bool canThrow(const std::string &fname) const;

public:
  explicit InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m, ValueEncoder &v,
                         LLVMTypes &t, ThreadsafeCompilerState &s, CodeGen &cg);

  llvm::Module &getLLVMModule() { return theModule; }

  llvm::Value *invokeRaw(const std::string &fname, llvm::FunctionType *type,
                         const std::vector<llvm::Value *> &args,
                         CleanupChainGuard *guard = nullptr);

  llvm::Value *invokeRaw(llvm::Value *fpointer, llvm::FunctionType *type,
                         const std::vector<llvm::Value *> &args,
                         CleanupChainGuard *guard = nullptr);

  TypedValue invokeRuntime(const std::string &fname,
                           const ObjectTypeSet *retValType,
                           const std::vector<ObjectTypeSet> &argTypes,
                           const std::vector<TypedValue> &args,
                           const bool isVariadic = false,
                           CleanupChainGuard *guard = nullptr);

  TypedValue generateIntrinsic(const IntrinsicDescription &id,
                               const std::vector<TypedValue> &args,
                               CleanupChainGuard *guard = nullptr);

  TypedValue generateInstanceCall(
      const std::string &methodName, TypedValue instance,
      const std::vector<TypedValue> &args, CleanupChainGuard *guard = nullptr,
      const clojure::rt::protobuf::bytecode::Node *node = nullptr);

  ObjectTypeSet predictInstanceCallType(const std::string &methodName,
                                        const ObjectTypeSet &instanceType,
                                        const std::vector<ObjectTypeSet> &args);

  ObjectTypeSet foldIntrinsic(const IntrinsicDescription &id,
                              const std::vector<ObjectTypeSet> &args);

  TypedValue
  generateStaticInvoke(TypedValue fn, const std::vector<TypedValue> &args,
                 CleanupChainGuard *guard = nullptr,
                 const clojure::rt::protobuf::bytecode::Node *node = nullptr);

  TypedValue
  generateDynamicInvoke(TypedValue fn, const std::vector<TypedValue> &args,
                 CleanupChainGuard *guard = nullptr,
                 const clojure::rt::protobuf::bytecode::Node *node = nullptr);
};

} // namespace rt

#endif // INVOKE_MANAGER_H
