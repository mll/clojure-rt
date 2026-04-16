#include "../../tools/RTValueWrapper.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "llvm/IR/MDBuilder.h"
#include <atomic>
#include <sstream>

using namespace llvm;
using namespace std;

namespace rt {

static std::atomic<uint64_t> globalIcCounter{0};

TypedValue InvokeManager::generateVarInvoke(
    TypedValue varObj, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {

  auto &TheContext = theModule.getContext();
  Function *currentFn = this->builder.GetInsertBlock()->getParent();
  size_t argCount = args.size();

  // 1. Get Var pointer (unboxed)
  Value *varPtr = valueEncoder.unboxPointer(varObj).value;

  // 2. Load current value FROM VAR (WITHOUT RETAIN/RELEASE - Var_peek)
  FunctionType *peekSig =
      FunctionType::get(types.RT_valueTy, {types.ptrTy}, false);
  Value *currentVal = invokeRaw("Var_peek", peekSig, {varPtr}, guard, true);

  // 3. Setup IC slot: { RTValue key, FunctionMethod* value }
  auto *slotTy = StructType::get(TheContext, {types.RT_valueTy, types.ptrTy});
  std::stringstream ss;
  ss << "__var_ic_slot_" << globalIcCounter.fetch_add(1);
  std::string slotName = ss.str();
  icSlotNames.push_back(slotName);

  // Initial value: { 0, nullptr }
  auto *init = ConstantStruct::get(
      slotTy, {ConstantInt::get(types.RT_valueTy, 0),
               ConstantPointerNull::get(cast<PointerType>(types.ptrTy))});

  auto *slotGlobal = new GlobalVariable(
      theModule, slotTy, false, GlobalValue::ExternalLinkage, init, slotName);
  slotGlobal->setAlignment(Align(16)); // For atomic 128-bit access

  // 4. Load from IC slot (Atomic 128-bit load)
  Type *i128Ty = IntegerType::get(TheContext, 128);
  LoadInst *pair =
      builder.CreateAlignedLoad(i128Ty, slotGlobal, Align(16), "var_ic_pair");
  pair->setAtomic(AtomicOrdering::Acquire);

  Value *cachedKey = builder.CreateTrunc(pair, types.i64Ty, "cached_key");
  Value *cachedMethodVal128 = builder.CreateLShr(pair, 64);
  Value *cachedMethodVal64 = builder.CreateTrunc(
      cachedMethodVal128, types.i64Ty, "cached_method_val64");
  Value *cachedMethod =
      builder.CreateIntToPtr(cachedMethodVal64, types.ptrTy, "cached_method");

  // 5. Compare and Branch
  BasicBlock *fastPath =
      BasicBlock::Create(TheContext, "var_ic_hit", currentFn);
  BasicBlock *slowPath =
      BasicBlock::Create(TheContext, "var_ic_miss", currentFn);
  BasicBlock *callBB = BasicBlock::Create(TheContext, "var_ic_call", currentFn);

  Value *isHit = builder.CreateICmpEQ(cachedKey, currentVal);
  builder.CreateCondBr(isHit, fastPath, slowPath);

  // --- SLOW PATH ---
  builder.SetInsertPoint(slowPath);
  // FunctionMethod* VarCallSlowPath(void* slot, RTValue currentVal, uint64_t
  // argCount)
  FunctionType *slowPathSig = FunctionType::get(
      types.ptrTy, {types.ptrTy, types.RT_valueTy, types.i64Ty}, false);
  Value *newMethod = invokeRaw(
      "VarCallSlowPath", slowPathSig,
      {slotGlobal, currentVal, builder.getInt64(argCount)}, guard, false);
  BasicBlock *slowPathEnd = builder.GetInsertBlock();
  builder.CreateBr(callBB);

  // --- FAST PATH ---
  builder.SetInsertPoint(fastPath);
  builder.CreateBr(callBB);

  // --- CALL SITE ---
  builder.SetInsertPoint(callBB);
  PHINode *methodToCall = builder.CreatePHI(types.ptrTy, 2, "method_to_call");
  methodToCall->addIncoming(cachedMethod, fastPath);
  methodToCall->addIncoming(newMethod, slowPathEnd);

  // Use the generalized helper
  return generateRawMethodCall(
      methodToCall, TypedValue(ObjectTypeSet::dynamicType(), currentVal), args,
      guard, node);
}

} // namespace rt
