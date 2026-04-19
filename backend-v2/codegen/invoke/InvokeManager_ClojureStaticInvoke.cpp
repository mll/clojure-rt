#include "../../bridge/Exceptions.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "codegen/TypedValue.h"
#include "runtime/ObjectProto.h"
#include "types/ConstantFunction.h"
#include "types/ObjectTypeSet.h"
#include "llvm/IR/Value.h"
#include <vector>

namespace rt {

/* Leaves fn at +1 only on happy path, needs additional happy path cleanup */
TypedValue InvokeManager::generateStaticInvoke(
    TypedValue fn, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard, const clojure::rt::protobuf::bytecode::Node *node,
    const std::vector<TypedValue> &extraCleanup) {
  size_t argCount = args.size();
  ConstantFunction *cf =
      dynamic_cast<ConstantFunction *>(fn.type.getConstant());

  ConstantMethod *match = nullptr;

  // Look for exact match
  for (auto &m : cf->methods) {
    if (!m.isVariadic && m.fixedArity == (int)argCount) {
      match = &m;
      break;
    }
  }

  // Look for variadic match if no exact match
  if (!match) {
    for (auto &m : cf->methods) {
      if (m.isVariadic && (int)argCount >= m.fixedArity) {
        match = &m;
        break;
      }
    }
  }

  if (!match) {
    throwCodeGenerationException("No matching method found for function",
                                 *node);
  }

  // --- Static Call Path ---
  llvm::Function *currentFn = builder.GetInsertBlock()->getParent();

  // A. Allocate Frame on stack
  int overflowArgs = std::max(0, (int)argCount - 5);

  // Calculate total size in the compiler (since overflowArgs is constant
  // at the call site) frameTy has {ptr, ptr, i64, i32, i32, [0 x i64]}
  const llvm::DataLayout &DL = theModule.getDataLayout();
  uint64_t frameBaseSize = DL.getTypeAllocSize(types.frameTy);
  uint64_t elementSize = DL.getTypeAllocSize(types.i64Ty);
  uint64_t totalSize =
      frameBaseSize + (static_cast<uint64_t>(overflowArgs) * elementSize);

  llvm::Value *framePtrRaw = builder.CreateAlloca(
      types.i8Ty, llvm::ConstantInt::get(types.i64Ty, totalSize), "frame_raw");
  llvm::Value *framePtr =
      builder.CreateBitCast(framePtrRaw, types.ptrTy, "frame_ptr");

  // B. Initialize Frame fields
  // 1. leafFrame (chain to current function's frame)
  // Current function's Arg 0 is the Frame*, but ONLY if it's a baseline
  // method. Top-level functions have 0 arguments.
  llvm::Value *currentFrame =
      currentFn->arg_size() > 0
          ? static_cast<llvm::Value *>(&*currentFn->arg_begin())
          : static_cast<llvm::Value *>(llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(types.ptrTy)));
  builder.CreateStore(currentFrame,
                      types.getFrameLeafFramePtr(builder, framePtr));

  // 2. method (pointer to FunctionMethod struct in ClojureFunction
  // object)
  llvm::Value *unboxedFn = valueEncoder.unboxPointer(fn).value;
  uint64_t methodsOffset = types.getFunctionMethodsOffset(DL);
  llvm::Value *methodsBase = builder.CreateInBoundsGEP(
      types.i8Ty, unboxedFn,
      {llvm::ConstantInt::get(types.i64Ty, methodsOffset)});

  uint64_t methodSize = DL.getTypeAllocSize(types.methodTy);
  llvm::Value *methodPtrRaw = builder.CreateInBoundsGEP(
      types.i8Ty, methodsBase,
      {llvm::ConstantInt::get(
          types.i64Ty, match->index * methodSize)}); // sizeof(FunctionMethod)
  llvm::Value *methodPtr = builder.CreateBitCast(methodPtrRaw, types.ptrTy);
  builder.CreateStore(methodPtr,
                      types.getFrameMethodPtr(builder, framePtr));

  // 3. self (boxed function object)
  builder.CreateStore(valueEncoder.box(fn).value,
                      types.getFrameSelfPtr(builder, framePtr));

  // 4. variadicSeq & locals
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(framePtr); // Arg 0: Frame*

  if (match->isVariadic) {
    // Pack surplus into variadicSeq
    int fixedPart = match->fixedArity;
    int surplus = (int)argCount - fixedPart;

    if (surplus > 0) {
      // Use runtime helper to create list: RT_createListFromArray(int32_t
      // count, RTValue* args)
      llvm::Value *surplusArray = builder.CreateAlloca(
          types.RT_valueTy, llvm::ConstantInt::get(types.i64Ty, surplus));
      for (int i = 0; i < surplus; ++i) {
        llvm::Value *argVal = valueEncoder.box(args[fixedPart + i]).value;
        builder.CreateStore(argVal,
                            builder.CreateInBoundsGEP(
                                types.RT_valueTy, surplusArray,
                                {llvm::ConstantInt::get(types.i64Ty, i)}));
      }
      llvm::FunctionType *listHelperTy = llvm::FunctionType::get(
          types.RT_valueTy, {types.i32Ty, types.ptrTy}, false);
      llvm::Value *vSeq = invokeRaw(
          "RT_createListFromArray", listHelperTy,
          {llvm::ConstantInt::get(types.i32Ty, surplus), surplusArray}, guard,
          false);
      builder.CreateStore(vSeq,
                          types.getFrameVariadicSeqPtr(builder, framePtr));
    } else {
      builder.CreateStore(valueEncoder.boxNil().value,
                          types.getFrameVariadicSeqPtr(builder, framePtr));
    }
  } else {
    builder.CreateStore(valueEncoder.boxNil().value,
                        types.getFrameVariadicSeqPtr(builder, framePtr));
  }

  // 5. bailoutEntryIndex & localsCount
  builder.CreateStore(llvm::ConstantInt::get(types.i32Ty, 0),
                      types.getFrameBailoutEntryIndexPtr(builder, framePtr));
  builder.CreateStore(llvm::ConstantInt::get(types.i32Ty, overflowArgs),
                      types.getFrameLocalsCountPtr(builder, framePtr));

  // 6. locals (overflow args > 5)
  for (int i = 0; i < overflowArgs; ++i) {
    llvm::Value *localPtr = types.getFrameLocalPtr(builder, framePtr, (uint32_t)i);
    builder.CreateStore(valueEncoder.box(args[5 + i]).value, localPtr);
  }

  // C. Prepare Call Arguments (first 5 args, boxed)
  for (int i = 0; i < 5; ++i) {
    if (i < (int)argCount) {
      callArgs.push_back(valueEncoder.box(args[i]).value);
    } else {
      callArgs.push_back(valueEncoder.boxNil().value);
    }
  }

  // D. Perform Call
  llvm::Value *res = invokeRaw(match->implementation, types.baselineFunctionTy,
                               callArgs, guard, true, extraCleanup);

  return TypedValue(ObjectTypeSet::all(), res);
}

TypedValue InvokeManager::generateStaticKeywordInvoke(
    TypedValue keyword, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();

  if (argCount != 1 && argCount != 2) {
    throwCodeGenerationException("Keyword invoke must have 1 or 2 arguments",
                                 *node);
  }

  if (args[0].type.isDetermined()) {
    if (args[0].type.determinedType() == persistentArrayMapType) {
      std::vector<TypedValue> mapArgs;
      mapArgs.push_back(keyword);
      if (argCount == 2) {
        mapArgs.push_back(args[1]);
      }

      return generateStaticMapInvoke(args[0], mapArgs, guard, node);
    } else {
      // Optimization: NOT a map. We know the result is nil (1-arg) or default
      // (2-arg).
      codeGen.getMemoryManagement().dynamicRelease(args[0]);
      if (argCount == 1) {
        return TypedValue(ObjectTypeSet(nilType), valueEncoder.boxNil().value);
      } else {
        // args[1] is already at +1 from codegen, so we just return it.
        return args[1];
      }
    }
  }
  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(valueEncoder.box(keyword).value); // self
  callArgs.push_back(valueEncoder.box(args[0]).value); // obj/map

  if (argCount == 2) {
    callArgs.push_back(valueEncoder.box(args[1]).value); // default
  } else {
    callArgs.push_back(valueEncoder.boxNil().value); // dummy default
  }

  llvm::FunctionType *invokeTy = llvm::FunctionType::get(
      types.RT_valueTy, {types.RT_valueTy, types.RT_valueTy, types.RT_valueTy},
      false);

  llvm::Value *res =
      invokeRaw("Keyword_invoke", invokeTy, callArgs, guard, true);
  return TypedValue(ObjectTypeSet::dynamicType(), res);
}

TypedValue InvokeManager::generateStaticMapInvoke(
    TypedValue map, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();
  if (argCount != 1 && argCount != 2) {
    throwCodeGenerationException("Map invoke must have 1 or 2 arguments", *node);
  }

  std::vector<ObjectTypeSet> argTypes;
  argTypes.push_back(ObjectTypeSet(persistentArrayMapType));
  argTypes.push_back(ObjectTypeSet::dynamicType());

  std::vector<TypedValue> callArgs;
  callArgs.push_back(map);
  callArgs.push_back(args[0]);
  TypedValue res = invokeRuntime("PersistentArrayMap_get", nullptr, argTypes,
                                 callArgs, false, guard);
  if (argCount == 2) {
    llvm::Function *currentFn = builder.GetInsertBlock()->getParent();
    llvm::Value *isNil =
        builder.CreateICmpEQ(res.value, valueEncoder.boxNil().value, "is_nil");
    llvm::BasicBlock *thenBB =
        llvm::BasicBlock::Create(theModule.getContext(), "res_nil", currentFn);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(
        theModule.getContext(), "res_not_nil", currentFn);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
        theModule.getContext(), "res_merge", currentFn);

    builder.CreateCondBr(isNil, thenBB, elseBB);

    builder.SetInsertPoint(thenBB);
    llvm::Value *defaultVal = valueEncoder.box(args[1]).value;
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(elseBB);
    codeGen.getMemoryManagement().dynamicRelease(args[1]);
    builder.CreateBr(mergeBB);

    builder.SetInsertPoint(mergeBB);
    llvm::PHINode *phi = builder.CreatePHI(types.RT_valueTy, 2);
    phi->addIncoming(defaultVal, thenBB);
    phi->addIncoming(res.value, elseBB);
    return TypedValue(ObjectTypeSet::dynamicType(), phi);
  } else {
    return res;
  }
}

TypedValue InvokeManager::generateStaticVectorInvoke(
    TypedValue vector, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();

  if (argCount != 1) {
    throwCodeGenerationException("Vector invoke must have 1 arguments", *node);
  }

  std::vector<TypedValue> callArgs;
  callArgs.push_back(vector);  // self
  callArgs.push_back(args[0]); // index

  std::vector<ObjectTypeSet> argTypes;
  argTypes.push_back(ObjectTypeSet(persistentVectorType));

  if (args[0].type.isDetermined() &&
      args[0].type.determinedType() == integerType) {
    argTypes.push_back(ObjectTypeSet(integerType));
    return invokeRuntime("PersistentVector_nth", nullptr, argTypes, callArgs,
                         false, guard);
  } else {
    argTypes.push_back(ObjectTypeSet::dynamicType());
  }

  return invokeRuntime("PersistentVector_dynamic_nth", nullptr, argTypes,
                       callArgs, false, guard);
}

} // namespace rt
