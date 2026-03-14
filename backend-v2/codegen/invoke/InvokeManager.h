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
class ShadowStackGuard;

using IntrinsicCall = std::function<llvm::Value *(llvm::IRBuilder<> &,
                                                  std::vector<llvm::Value *>)>;
using TypeIntrinsicCall =
    std::function<ObjectTypeSet(const std::vector<ObjectTypeSet> &)>;
using GenericIntrinsicCall = std::function<llvm::Value *(
    llvm::IRBuilder<> &, const std::vector<TypedValue> &)>;

class InvokeManager {
  friend void registerMathIntrinsics(InvokeManager &mgr);
  friend void registerCmpIntrinsics(InvokeManager &mgr);

private:
  llvm::IRBuilder<> &builder;
  llvm::Module &theModule;
  ValueEncoder &valueEncoder;
  LLVMTypes &types;
  std::unordered_map<std::string, IntrinsicCall> intrinsics;
  std::unordered_map<std::string, GenericIntrinsicCall> genericIntrinsics;
  std::unordered_map<std::string, TypeIntrinsicCall> typeIntrinsics;
  llvm::BasicBlock *landingPad = nullptr;

  // Folding Helpers
  mpz_ptr getZ(const ObjectTypeSet &t);
  mpq_ptr getQ(const ObjectTypeSet &t);
  double getD(const ObjectTypeSet &t);
  ObjectTypeSet createZ(mpz_ptr val);
  ObjectTypeSet createQ(mpq_ptr val);


public:
  explicit InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m, ValueEncoder &v,
                         LLVMTypes &t, ThreadsafeCompilerState &s);

  TypedValue invokeRuntime(const std::string &fname,
                           const ObjectTypeSet *retValType,
                           const std::vector<ObjectTypeSet> &argTypes,
                           const std::vector<TypedValue> &args,
                           const bool isVariadic = false,
                           ShadowStackGuard *guard = nullptr);

  TypedValue generateIntrinsic(const IntrinsicDescription &id,
                               const std::vector<TypedValue> &args,
                               ShadowStackGuard *guard = nullptr);

  ObjectTypeSet foldIntrinsic(const IntrinsicDescription &id,
                              const std::vector<ObjectTypeSet> &args);

  void setLandingPad(llvm::BasicBlock *lp) { landingPad = lp; }
};


} // namespace rt

#endif // INVOKE_MANAGER_H
