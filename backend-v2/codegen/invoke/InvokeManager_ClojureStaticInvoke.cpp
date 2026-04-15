#include "../../bridge/Exceptions.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "types/ConstantFunction.h"

namespace rt {

TypedValue InvokeManager::generateStaticInvoke(
    TypedValue fn, const std::vector<TypedValue> &args, CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node,
    const std::vector<TypedValue> &extraCleanup) {
  ObjectTypeSet fnType = fn.type;
  size_t argCount = args.size();

  // 1. Check for ConstantFunction optimization (Static Case)
  if (fnType.isDetermined() && fnType.getConstant()) {
    if (ConstantFunction *cf =
            dynamic_cast<ConstantFunction *>(fnType.getConstant())) {
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

      if (match) {
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
            types.i8Ty, llvm::ConstantInt::get(types.i64Ty, totalSize),
            "frame_raw");
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
        builder.CreateStore(
            currentFrame, builder.CreateStructGEP(types.frameTy, framePtr, 0));

        // 2. method (pointer to FunctionMethod struct in ClojureFunction
        // object)
        llvm::Value *unboxedFn = valueEncoder.unboxPointer(fn).value;
        uint64_t methodsOffset =
            DL.getStructLayout(types.clojureFunctionTy)->getElementOffset(5);
        llvm::Value *methodsBase = builder.CreateInBoundsGEP(
            types.i8Ty, unboxedFn,
            {llvm::ConstantInt::get(types.i64Ty, methodsOffset)});

        uint64_t methodSize = DL.getTypeAllocSize(types.methodTy);
        llvm::Value *methodPtrRaw = builder.CreateInBoundsGEP(
            types.i8Ty, methodsBase,
            {llvm::ConstantInt::get(types.i64Ty,
                                    match->index *
                                        methodSize)}); // sizeof(FunctionMethod)
        llvm::Value *methodPtr =
            builder.CreateBitCast(methodPtrRaw, types.ptrTy);
        builder.CreateStore(
            methodPtr, builder.CreateStructGEP(types.frameTy, framePtr, 1));

        // 3. variadicSeq & locals
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
              builder.CreateStore(
                  argVal, builder.CreateInBoundsGEP(
                              types.RT_valueTy, surplusArray,
                              {llvm::ConstantInt::get(types.i64Ty, i)}));
            }
            llvm::FunctionType *listHelperTy = llvm::FunctionType::get(
                types.RT_valueTy, {types.i32Ty, types.ptrTy}, false);
            llvm::Value *vSeq = invokeRaw(
                "RT_createListFromArray", listHelperTy,
                {llvm::ConstantInt::get(types.i32Ty, surplus), surplusArray},
                guard, false);
            if (guard) {
              guard->push(
                  TypedValue(ObjectTypeSet(persistentListType, true), vSeq));
            }
            builder.CreateStore(
                vSeq, builder.CreateStructGEP(types.frameTy, framePtr, 2));
          } else {
            builder.CreateStore(
                valueEncoder.boxNil().value,
                builder.CreateStructGEP(types.frameTy, framePtr, 2));
          }
        } else {
          builder.CreateStore(
              valueEncoder.boxNil().value,
              builder.CreateStructGEP(types.frameTy, framePtr, 2));
        }

        // 4. bailoutEntryIndex & localsCount
        builder.CreateStore(
            llvm::ConstantInt::get(types.i32Ty, 0),
            builder.CreateStructGEP(types.frameTy, framePtr, 3));
        builder.CreateStore(
            llvm::ConstantInt::get(types.i32Ty, overflowArgs),
            builder.CreateStructGEP(types.frameTy, framePtr, 4));

        // 5. locals (overflow args > 5)
        for (int i = 0; i < overflowArgs; ++i) {
          llvm::Value *localPtr = builder.CreateInBoundsGEP(
              types.frameTy, framePtr,
              {builder.getInt32(0), builder.getInt32(5), builder.getInt32(i)},
              "arg_ptr");
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
        llvm::Value *res = invokeRaw(match->implementation,
                                     types.baselineFunctionTy, callArgs, guard,
                                     true, extraCleanup);

        return TypedValue(ObjectTypeSet::all(), res);
      } else {
        // Arity mismatch for constant function
        llvm::FunctionType *arityExTy = llvm::FunctionType::get(
            types.voidTy, {types.i32Ty, types.i32Ty}, false);
        invokeRaw("throwArityException_C", arityExTy,
                  {builder.getInt32(-1), builder.getInt32((int)argCount)},
                  guard, false);
        return TypedValue(ObjectTypeSet::all(),
                          llvm::UndefValue::get(types.RT_valueTy));
      }
    }
  }

  throwInternalInconsistencyException(
      "generateStaticInvoke called on non-constant function");
}

TypedValue InvokeManager::generateStaticKeywordInvoke(
    TypedValue keyword, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard, const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();
  llvm::Value *boxedKeyword = valueEncoder.box(keyword).value;

  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(boxedKeyword); // self
  callArgs.push_back(valueEncoder.box(args[0]).value); // obj/map

  if (argCount == 2) {
    callArgs.push_back(valueEncoder.box(args[1]).value); // default
  } else {
    callArgs.push_back(valueEncoder.boxNil().value); // dummy default
  }
  callArgs.push_back(builder.getInt32((int)argCount));

  llvm::FunctionType *invokeTy = llvm::FunctionType::get(
      types.RT_valueTy, {types.RT_valueTy, types.RT_valueTy, types.RT_valueTy, types.i32Ty}, false);

  llvm::Value *res = invokeRaw("Keyword_invoke", invokeTy, callArgs, guard, true);
  return TypedValue(ObjectTypeSet::dynamicType(), res);
}

TypedValue InvokeManager::generateStaticMapInvoke(
    TypedValue map, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard, const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();
  llvm::Value *boxedMap = valueEncoder.box(map).value;

  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(boxedMap); // self
  callArgs.push_back(valueEncoder.box(args[0]).value); // key

  if (argCount == 2) {
    callArgs.push_back(valueEncoder.box(args[1]).value); // default
  } else {
    callArgs.push_back(valueEncoder.boxNil().value); // dummy default
  }
  callArgs.push_back(builder.getInt32((int)argCount));

  llvm::FunctionType *invokeTy = llvm::FunctionType::get(
      types.RT_valueTy, {types.RT_valueTy, types.RT_valueTy, types.RT_valueTy, types.i32Ty}, false);

  llvm::Value *res = invokeRaw("PersistentArrayMap_invoke", invokeTy, callArgs, guard, true);
  return TypedValue(ObjectTypeSet::dynamicType(), res);
}

TypedValue InvokeManager::generateStaticVectorInvoke(
    TypedValue vector, const std::vector<TypedValue> &args,
    CleanupChainGuard *guard, const clojure::rt::protobuf::bytecode::Node *node) {
  size_t argCount = args.size();
  llvm::Value *boxedVector = valueEncoder.box(vector).value;

  if (argCount != 1) {
    // We could throw here or let the runtime function throw
  }

  std::vector<llvm::Value *> callArgs;
  callArgs.push_back(boxedVector); // self
  callArgs.push_back(valueEncoder.box(args[0]).value); // index
  callArgs.push_back(builder.getInt32((int)argCount));

  llvm::FunctionType *invokeTy = llvm::FunctionType::get(
      types.RT_valueTy, {types.RT_valueTy, types.RT_valueTy, types.i32Ty}, false);

  llvm::Value *res = invokeRaw("PersistentVector_invoke", invokeTy, callArgs, guard, true);
  return TypedValue(ObjectTypeSet::dynamicType(), res);
}

} // namespace rt
