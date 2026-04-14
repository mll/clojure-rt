#include "../../bridge/Exceptions.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "types/ConstantFunction.h"

namespace rt {

TypedValue InvokeManager::generateDynamicInvoke(
    TypedValue fn, const std::vector<TypedValue> &args, CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node,
    const std::vector<TypedValue> &extraCleanup) {
  size_t argCount = args.size();
  llvm::Function *currentFn = builder.GetInsertBlock()->getParent();

  // 1. Extract FunctionMethod* at runtime
  // Function_extractMethod(RTValue funObj, uword_t argCount)
  llvm::Value *boxedFn = valueEncoder.box(fn).value;
  llvm::FunctionType *extractMethodTy = llvm::FunctionType::get(
      types.ptrTy, {types.RT_valueTy, types.wordTy}, false);
  llvm::Value *methodPtr =
      invokeRaw("Function_extractMethod", extractMethodTy,
                {boxedFn, llvm::ConstantInt::get(types.wordTy, argCount)},
                guard, false);

  // 2. Load isVariadic and fixedArity from FunctionMethod*
  // struct FunctionMethod { u8 index, u8 isVariadic, u8 fixedArity, ... }
  llvm::Value *isVariadicRaw = builder.CreateLoad(
      types.i8Ty, builder.CreateStructGEP(types.methodTy, methodPtr, 1));
  llvm::Value *fixedArityRaw = builder.CreateLoad(
      types.i8Ty, builder.CreateStructGEP(types.methodTy, methodPtr, 2));
  
  llvm::Value *isVariadic = builder.CreateICmpNE(isVariadicRaw, builder.getInt8(0));

  // 3. Allocate Frame on stack
  int maxOverflow = std::max(0, (int)argCount - 5);
  const llvm::DataLayout &DL = theModule.getDataLayout();
  uint64_t frameBaseSize = DL.getTypeAllocSize(types.frameTy);
  uint64_t elementSize = DL.getTypeAllocSize(types.i64Ty);
  uint64_t totalSize =
      frameBaseSize + (static_cast<uint64_t>(maxOverflow) * elementSize);

  llvm::Value *framePtrRaw = builder.CreateAlloca(
      types.i8Ty, llvm::ConstantInt::get(types.i64Ty, totalSize),
      "frame_raw");
  llvm::Value *framePtr =
      builder.CreateBitCast(framePtrRaw, types.ptrTy, "frame_ptr");

  // 4. Initialize Frame fields
  // leafFrame
  llvm::Value *currentFrame =
      currentFn->arg_size() > 0
          ? static_cast<llvm::Value *>(&*currentFn->arg_begin())
          : static_cast<llvm::Value *>(llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(types.ptrTy)));
  builder.CreateStore(
      currentFrame, builder.CreateStructGEP(types.frameTy, framePtr, 0));

  // method
  builder.CreateStore(
      methodPtr, builder.CreateStructGEP(types.frameTy, framePtr, 1));

  // variadicSeq (using RT_packVariadic helper)
  llvm::Value *argsArray = builder.CreateAlloca(
      types.RT_valueTy, llvm::ConstantInt::get(types.i64Ty, argCount));
  for (int i = 0; i < (int)argCount; ++i) {
    llvm::Value *argVal = valueEncoder.box(args[i]).value;
    builder.CreateStore(argVal, builder.CreateInBoundsGEP(
                                    types.RT_valueTy, argsArray,
                                    {llvm::ConstantInt::get(types.i64Ty, i)}));
  }

  llvm::BasicBlock *variadicBB =
      llvm::BasicBlock::Create(theModule.getContext(), "variadic_pack", currentFn);
  llvm::BasicBlock *noVariadicBB =
      llvm::BasicBlock::Create(theModule.getContext(), "no_variadic", currentFn);
  llvm::BasicBlock *packingDoneBB =
      llvm::BasicBlock::Create(theModule.getContext(), "packing_done", currentFn);

  builder.CreateCondBr(isVariadic, variadicBB, noVariadicBB);

  // --- Variadic Path ---
  builder.SetInsertPoint(variadicBB);
  llvm::FunctionType *packVariadicTy = llvm::FunctionType::get(
      types.RT_valueTy, {types.wordTy, types.ptrTy, types.wordTy}, false);
  
  llvm::Value *vSeqRaw = invokeRaw(
      "RT_packVariadic", packVariadicTy,
      {llvm::ConstantInt::get(types.wordTy, argCount), argsArray,
       builder.CreateZExt(fixedArityRaw, types.wordTy)},
      guard, false);
      
  builder.CreateStore(vSeqRaw, builder.CreateStructGEP(types.frameTy, framePtr, 2));
  
  llvm::BasicBlock *variadicExitBB = builder.GetInsertBlock();
  builder.CreateBr(packingDoneBB);

  // --- No Variadic Path ---
  builder.SetInsertPoint(noVariadicBB);
  llvm::Value *nilVal = valueEncoder.boxNil().value;
  builder.CreateStore(nilVal, builder.CreateStructGEP(types.frameTy, framePtr, 2));
  builder.CreateBr(packingDoneBB);

  // --- Merge ---
  builder.SetInsertPoint(packingDoneBB);
  llvm::PHINode *vSeqPhi = builder.CreatePHI(types.RT_valueTy, 2, "vseq_phi");
  vSeqPhi->addIncoming(vSeqRaw, variadicExitBB);
  vSeqPhi->addIncoming(nilVal, noVariadicBB);

  if (guard) {
      // Note: List returned by RT_packVariadic needs to be tracked for cleanup
      guard->push(TypedValue(ObjectTypeSet(persistentListType, true), vSeqPhi));
  }

  // bailoutEntryIndex
  builder.CreateStore(
      llvm::ConstantInt::get(types.i32Ty, 0),
      builder.CreateStructGEP(types.frameTy, framePtr, 3));

  // localsCount (overflow args > 5)
  builder.CreateStore(
      llvm::ConstantInt::get(types.i32Ty, maxOverflow),
      builder.CreateStructGEP(types.frameTy, framePtr, 4));

  // 5. Fill locals (overflow args > 5)
  for (int i = 0; i < maxOverflow; ++i) {
    llvm::Value *localPtr = builder.CreateInBoundsGEP(
        types.frameTy, framePtr,
        {builder.getInt32(0), builder.getInt32(5), builder.getInt32(i)},
        "arg_ptr");
    builder.CreateStore(valueEncoder.box(args[5 + i]).value, localPtr);
  }

  // 6. Prepare Call Arguments (first 5 args, boxed)
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(framePtr);
  for (int i = 0; i < 5; ++i) {
    if (i < (int)argCount) {
      callArgs.push_back(valueEncoder.box(args[i]).value);
    } else {
      callArgs.push_back(valueEncoder.boxNil().value);
    }
  }

  // 7. Perform Call
  llvm::Value *implPtr = builder.CreateLoad(
      types.ptrTy, builder.CreateStructGEP(types.methodTy, methodPtr, 4));
  llvm::Value *res = invokeRaw(implPtr, types.baselineFunctionTy, callArgs, guard,
                               true, extraCleanup);

  return TypedValue(ObjectTypeSet::all(), res);
}

} // namespace rt
