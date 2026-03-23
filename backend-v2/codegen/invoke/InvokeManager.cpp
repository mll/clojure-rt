#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include "../../types/ConstantBigInteger.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../../types/ConstantRatio.h"
#include "../CleanupChainGuard.h"
#include "../CodeGen.h"

#include "llvm/IR/MDBuilder.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <sstream>
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace std;

namespace rt {

// External registration functions
void registerMathIntrinsics(InvokeManager &mgr);
void registerCmpIntrinsics(InvokeManager &mgr);

InvokeManager::InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                             ValueEncoder &v, LLVMTypes &t,
                             ThreadsafeCompilerState &s, CodeGen &cg)
    : builder(b), theModule(m), valueEncoder(v), types(t), codeGen(cg),
      compilerState(s) {

  // Register domain intrinsics
  registerMathIntrinsics(*this);
  registerCmpIntrinsics(*this);
}

// Folding Helpers Implementation

mpz_ptr InvokeManager::getZ(const ObjectTypeSet &s) {
  auto *c = s.getConstant();
  if (!c)
    return nullptr;
  if (auto *i = dynamic_cast<ConstantInteger *>(c)) {
    mpz_ptr res = (mpz_ptr)malloc(sizeof(__mpz_struct));
    mpz_init_set_si(res, i->value);
    return res;
  }
  if (auto *b = dynamic_cast<ConstantBigInteger *>(c)) {
    mpz_ptr res = (mpz_ptr)malloc(sizeof(__mpz_struct));
    mpz_init_set(res, b->value);
    return res;
  }
  return nullptr;
}

mpq_ptr InvokeManager::getQ(const ObjectTypeSet &s) {
  auto *c = s.getConstant();
  if (!c)
    return nullptr;
  if (auto *i = dynamic_cast<ConstantInteger *>(c)) {
    mpq_ptr res = (mpq_ptr)malloc(sizeof(__mpq_struct));
    mpq_init(res);
    mpq_set_si(res, i->value, 1);
    mpq_canonicalize(res);
    return res;
  }
  if (auto *b = dynamic_cast<ConstantBigInteger *>(c)) {
    mpq_ptr res = (mpq_ptr)malloc(sizeof(__mpq_struct));
    mpq_init(res);
    mpq_set_z(res, b->value);
    mpq_canonicalize(res);
    return res;
  }
  if (auto *r = dynamic_cast<ConstantRatio *>(c)) {
    mpq_ptr res = (mpq_ptr)malloc(sizeof(__mpq_struct));
    mpq_init(res);
    mpq_set(res, r->value);
    return res;
  }
  return nullptr;
}

double InvokeManager::getD(const ObjectTypeSet &s) {
  auto *c = s.getConstant();
  if (!c)
    return 0.0;
  if (auto *d = dynamic_cast<ConstantDouble *>(c))
    return d->value;
  if (auto *i = dynamic_cast<ConstantInteger *>(c))
    return (double)i->value;
  if (auto *b = dynamic_cast<ConstantBigInteger *>(c))
    return mpz_get_d(b->value);
  if (auto *r = dynamic_cast<ConstantRatio *>(c))
    return mpq_get_d(r->value);
  return 0.0;
}

ObjectTypeSet InvokeManager::createZ(mpz_ptr val) {
  if (mpz_fits_slong_p(val)) {
    return ObjectTypeSet(integerType, false,
                         new ConstantInteger(mpz_get_si(val)));
  }
  return ObjectTypeSet(bigIntegerType, false, new ConstantBigInteger(val));
}

ObjectTypeSet InvokeManager::createQ(mpq_ptr val) {
  mpq_canonicalize(val);
  if (mpz_cmp_si(mpq_denref(val), 1) == 0) {
    return createZ(mpq_numref(val));
  }
  return ObjectTypeSet(ratioType, false, new ConstantRatio(val));
}

TypedValue InvokeManager::generateIntrinsic(const IntrinsicDescription &id,
                                            const vector<TypedValue> &args,
                                            CleanupChainGuard *guard) {
  if (id.type == CallType::Intrinsic) {
    auto git = genericIntrinsics.find(id.symbol);
    if (git != genericIntrinsics.end()) {
      return TypedValue(id.returnType, git->second(builder, args, guard));
    }

    auto it = intrinsics.find(id.symbol);
    if (it == intrinsics.end()) {
      throwInternalInconsistencyException("Intrinsic '" + id.symbol +
                                          "' does not exist.");
    }
    std::vector<llvm::Value *> argVals;
    for (auto &arg : args)
      argVals.push_back(valueEncoder.unbox(arg).value);
    return TypedValue(id.returnType, it->second(builder, argVals));
  } else if (id.type == CallType::Call) {
    return invokeRuntime(id.symbol, &id.returnType, id.argTypes, args, false,
                         guard);
  } else {
    return invokeRuntime(id.symbol, &id.returnType, id.argTypes, args, false,
                         nullptr);
  }

  throwInternalInconsistencyException("Unsupported call type");
}

ObjectTypeSet InvokeManager::foldIntrinsic(const IntrinsicDescription &id,
                                           const vector<ObjectTypeSet> &args) {
  auto it = typeIntrinsics.find(id.symbol);
  if (it != typeIntrinsics.end()) {
    ObjectTypeSet folded = it->second(args);
    if (folded.isDetermined() && folded.getConstant()) {
      return folded;
    }
  }
  return id.returnType;
}

bool InvokeManager::canThrow(const std::string &fname) const {
  static const std::unordered_set<std::string> safeFunctions = {
      "retain",
      "release",
      "getType",
      "Object_promoteToShared",
      "Var_deref",
      "Var_hasRoot",
      "Var_isDynamic",
      "RT_boxInt32",
      "RT_boxDouble",
      "RT_boxBoolean",
      "RT_boxNil",
      "RT_unboxInt32",
      "RT_unboxDouble",
      "RT_unboxBoolean",

      "BigInteger_toDouble",
      "BigInteger_createFromInt",
      "BigInteger_add",
      "BigInteger_sub",
      "BigInteger_mul",
      "BigInteger_gte",
      "BigInteger_gt",
      "BigInteger_lt",
      "BigInteger_lte",
      "BigInteger_toString",

      "Ratio_toDouble",
      "Ratio_createFromInt",
      "Ratio_createFromBigInteger",
      "Ratio_add",
      "Ratio_sub",
      "Ratio_mul",
      "Ratio_gte",
      "Ratio_gt",
      "Ratio_lt",
      "Ratio_lte",
      "Ratio_toString",
  };
  return safeFunctions.find(fname) == safeFunctions.end();
}

llvm::Value *InvokeManager::invokeRaw(llvm::Value *fpointer,
                                      llvm::FunctionType *type,
                                      const std::vector<llvm::Value *> &args,
                                      CleanupChainGuard *guard) {
  BasicBlock *lpadToUse =
      codeGen.getMemoryManagement().getLandingPad(guard ? guard->size() : 0);
  Value *callResult;
  bool isVoid = type->getReturnType()->isVoidTy();
  if (lpadToUse) {
    BasicBlock *normalBB =
        BasicBlock::Create(theModule.getContext(), "invoke_normal",
                           builder.GetInsertBlock()->getParent());
    InvokeInst *inv = builder.CreateInvoke(
        type, fpointer, normalBB, lpadToUse, args,
        isVoid ? std::string("") : std::string("inv_pointer"));
    callResult = inv;

    // Attach branch weights to mark landing pad as cold.
    // normal path: 1000000, unwind path: 1
    MDBuilder mdb(theModule.getContext());
    inv->setMetadata(LLVMContext::MD_prof, mdb.createBranchWeights(1000000, 1));

    builder.SetInsertPoint(normalBB);
  } else {
    callResult = builder.CreateCall(
        type, fpointer, args,
        isVoid ? std::string("") : std::string("call_pointer"));
  }
  return callResult;
}

llvm::Value *InvokeManager::invokeRaw(const std::string &fname,
                                      llvm::FunctionType *type,
                                      const std::vector<llvm::Value *> &args,
                                      CleanupChainGuard *guard) {
  Function *toCall = theModule.getFunction(fname);
  if (!toCall) {
    toCall =
        Function::Create(type, Function::ExternalLinkage, fname, theModule);
  }
  BasicBlock *lpadToUse = canThrow(fname)
                              ? codeGen.getMemoryManagement().getLandingPad(
                                    guard ? guard->size() : 0)
                              : nullptr;
  Value *callResult;
  bool isVoid = type->getReturnType()->isVoidTy();
  if (lpadToUse) {
    BasicBlock *normalBB =
        BasicBlock::Create(theModule.getContext(), "invoke_normal",
                           builder.GetInsertBlock()->getParent());
    InvokeInst *inv =
        builder.CreateInvoke(toCall, normalBB, lpadToUse, args,
                             isVoid ? std::string("") : std::string("inv_") + fname);
    callResult = inv;

    // Attach branch weights to mark landing pad as cold.
    // normal path: 1000000, unwind path: 1
    MDBuilder mdb(theModule.getContext());
    inv->setMetadata(LLVMContext::MD_prof, mdb.createBranchWeights(1000000, 1));

    builder.SetInsertPoint(normalBB);
  } else {
    callResult = builder.CreateCall(
        toCall, args, isVoid ? std::string("") : std::string("call_") + fname);
  }
  return callResult;
}

TypedValue
InvokeManager::invokeRuntime(const std::string &fname,
                             const ObjectTypeSet *retValType,
                             const std::vector<ObjectTypeSet> &argTypes,
                             const std::vector<TypedValue> &args,
                             const bool isVariadic, CleanupChainGuard *guard) {
  std::vector<llvm::Type *> llvmTypes;
  for (auto &at : argTypes) {
    if (at.isBoxedType())
      llvmTypes.push_back(types.RT_valueTy);
    else
      llvmTypes.push_back(
          types.typeForType(ObjectTypeSet(at.determinedType())));
  }

  FunctionType *functionType = FunctionType::get(
      retValType && !retValType->isBoxedType()
          ? types.typeForType(ObjectTypeSet(retValType->determinedType()))
          : types.RT_valueTy,
      llvmTypes, isVariadic);

  std::vector<llvm::Value *> argVals;
  for (size_t i = 0; i < args.size(); i++) {
    auto &arg = args[i];
    if (i < argTypes.size()) {
      auto &argType = argTypes[i];
      if (!argType.isBoxedType()) {
        if (!arg.type.isDetermined()) {
          std::ostringstream oss;
          oss << "Internal error: Types mismatch for runtime function call: "
                 "for "
              << i << "th arg required is " << argType.toString()
              << " and found " << arg.type.toString() << ".";
          throwInternalInconsistencyException(oss.str());
        }
        argVals.push_back(valueEncoder.unbox(arg).value);
      } else {
        /* Each boxed is allowed */
        argVals.push_back(valueEncoder.box(arg).value);
      }
    } else {
      /* Varargs are always boxed */
      argVals.push_back(valueEncoder.box(arg).value);
    }
  }

  return TypedValue(retValType ? *retValType : ObjectTypeSet::all(),
                    invokeRaw(fname, functionType, argVals, guard));
}

} // namespace rt
