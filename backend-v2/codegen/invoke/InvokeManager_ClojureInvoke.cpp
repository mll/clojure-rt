#include "../CodeGen.h"
#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"

namespace rt {

ObjectTypeSet
InvokeManager::predictInvokeType(const ObjectTypeSet &fnType,
                                 const std::vector<ObjectTypeSet> &args) {
  // Currently we don't track return types in ConstantMethod.
  return ObjectTypeSet::dynamicType();
}

TypedValue
InvokeManager::generateInvoke(TypedValue fn, const std::vector<TypedValue> &args,
                              CleanupChainGuard *guard, const Node *node) {
  ObjectTypeSet fnType = fn.type;
  size_t argCount = args.size();

  // 1. Check for ConstantFunction optimization (Static Case)
  if (fnType.isDetermined() && fnType.getConstant()) {
    if (ConstantFunction *cf = dynamic_cast<ConstantFunction *>(fnType.getConstant())) {
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
        
        // Calculate total size in the compiler (since overflowArgs is constant at the call site)
        // frameTy has {ptr, ptr, i64, i32, i32, [0 x i64]}
        const llvm::DataLayout &DL = theModule.getDataLayout();
        uint64_t frameBaseSize = DL.getTypeAllocSize(types.frameTy);
        uint64_t elementSize = DL.getTypeAllocSize(types.i64Ty);
        uint64_t totalSize = frameBaseSize + (static_cast<uint64_t>(overflowArgs) * elementSize);
                              
        llvm::Value* framePtrRaw = builder.CreateAlloca(types.i8Ty, llvm::ConstantInt::get(types.i64Ty, totalSize), "frame_raw");
        llvm::Value* framePtr = builder.CreateBitCast(framePtrRaw, types.ptrTy, "frame_ptr");

        // B. Initialize Frame fields
        // 1. leafFrame (chain to current function's frame)
        // Current function's Arg 0 is the Frame*, but ONLY if it's a baseline method.
        // Top-level functions have 0 arguments.
        llvm::Value* currentFrame = currentFn->arg_size() > 0 
                                     ? static_cast<llvm::Value*>(&*currentFn->arg_begin()) 
                                     : static_cast<llvm::Value*>(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(types.ptrTy)));
        builder.CreateStore(currentFrame, builder.CreateStructGEP(types.frameTy, framePtr, 0));

        // 2. method (pointer to FunctionMethod struct in ClojureFunction object)
        llvm::Value* unboxedFn = valueEncoder.unboxPointer(fn).value;
        // offsetof(ClojureFunction, methods) is 40.
        llvm::Value* methodsBase = builder.CreateInBoundsGEP(types.i8Ty, unboxedFn, {llvm::ConstantInt::get(types.i64Ty, 40)});
        llvm::Value* methodPtrRaw = builder.CreateInBoundsGEP(types.i8Ty, methodsBase, {llvm::ConstantInt::get(types.i64Ty, match->index * 32)}); // sizeof(FunctionMethod) = 32
        llvm::Value* methodPtr = builder.CreateBitCast(methodPtrRaw, types.ptrTy);
        builder.CreateStore(methodPtr, builder.CreateStructGEP(types.frameTy, framePtr, 1));

        // 3. variadicSeq & locals
        std::vector<llvm::Value*> callArgs;
        callArgs.push_back(framePtr); // Arg 0: Frame*

        if (match->isVariadic) {
          // Pack surplus into variadicSeq
          int fixedPart = match->fixedArity;
          int surplus = (int)argCount - fixedPart;
          
          if (surplus > 0) {
            // Use runtime helper to create list: RT_createListFromArray(int32_t count, RTValue* args)
            llvm::Value* surplusArray = builder.CreateAlloca(types.RT_valueTy, llvm::ConstantInt::get(types.i64Ty, surplus));
            for (int i = 0; i < surplus; ++i) {
              llvm::Value* argVal = valueEncoder.box(args[fixedPart + i]).value;
              builder.CreateStore(argVal, builder.CreateInBoundsGEP(types.RT_valueTy, surplusArray, {llvm::ConstantInt::get(types.i64Ty, i)}));
            }
            llvm::FunctionType* listHelperTy = llvm::FunctionType::get(types.RT_valueTy, {types.i32Ty, types.ptrTy}, false);
            llvm::Value* vSeq = invokeRaw("RT_createListFromArray", listHelperTy, {llvm::ConstantInt::get(types.i32Ty, surplus), surplusArray}, guard);
            if (guard) {
              guard->push(TypedValue(ObjectTypeSet(persistentListType, true), vSeq));
            }
            builder.CreateStore(vSeq, builder.CreateStructGEP(types.frameTy, framePtr, 2));
          } else {
            builder.CreateStore(valueEncoder.boxNil().value, builder.CreateStructGEP(types.frameTy, framePtr, 2));
          }
        } else {
          builder.CreateStore(valueEncoder.boxNil().value, builder.CreateStructGEP(types.frameTy, framePtr, 2));
        }

        // 4. bailoutEntryIndex & localsCount
        builder.CreateStore(llvm::ConstantInt::get(types.i32Ty, 0), builder.CreateStructGEP(types.frameTy, framePtr, 3)); 
        builder.CreateStore(llvm::ConstantInt::get(types.i32Ty, overflowArgs), builder.CreateStructGEP(types.frameTy, framePtr, 4));

        // 5. locals (overflow args > 5)
        for (int i = 0; i < overflowArgs; ++i) {
            llvm::Value* localPtr = builder.CreateInBoundsGEP(
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
        llvm::Value* res = invokeRaw(match->implementation, types.baselineFunctionTy, callArgs, guard);
        
        return TypedValue(ObjectTypeSet::all(), res);
      }
    }
  }

  // 2. Fallback: Dynamic Dispatch (Generic Path)
  // For now, call a runtime helper: RT_invoke(funObj, argCount, argsArray)
  llvm::Value* argsArray = builder.CreateAlloca(types.RT_valueTy, llvm::ConstantInt::get(types.i64Ty, argCount));
  for (int i = 0; i < (int)argCount; ++i) {
    llvm::Value* argVal = valueEncoder.box(args[i]).value;
    builder.CreateStore(argVal, builder.CreateInBoundsGEP(types.RT_valueTy, argsArray, {llvm::ConstantInt::get(types.i64Ty, i)}));
  }

  llvm::FunctionType* invokeHelperTy = llvm::FunctionType::get(types.RT_valueTy, {types.i64Ty, types.ptrTy, types.i32Ty}, false);
  llvm::Value* res = invokeRaw("RT_invokeDynamic", invokeHelperTy, {valueEncoder.box(fn).value, argsArray, llvm::ConstantInt::get(types.i32Ty, (int)argCount)}, guard);

  return TypedValue(ObjectTypeSet::all(), res);
}

} // namespace rt
