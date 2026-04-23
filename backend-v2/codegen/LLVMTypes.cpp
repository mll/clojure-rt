#include "LLVMTypes.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include <cstdint>

using namespace llvm;

namespace rt {

LLVMTypes::LLVMTypes(LLVMContext &context) {
  i64Ty = Type::getInt64Ty(context);
  i32Ty = Type::getInt32Ty(context);
  i8Ty = Type::getInt8Ty(context);
  i1Ty = Type::getInt1Ty(context);
  doubleTy = Type::getDoubleTy(context);
  ptrTy = PointerType::getUnqual(context); // Opaque pointer in LLVM 15+
  wordTy = K_WORD_SIZE == 32 ? i32Ty : i64Ty;
  voidTy = Type::getVoidTy(context);
  RT_valueTy = i64Ty;

  std::vector<Type *> objectFields = {wordTy, i32Ty};
  if (K_WORD_SIZE == 8) {
#ifdef USE_MEMORY_BANKS
    objectFields.push_back(i8Ty);                    // bankId
    objectFields.push_back(ArrayType::get(i8Ty, 3)); // padding to 16
#else
    objectFields.push_back(i32Ty); // padding to 16
#endif
  }

  RT_objectTy = StructType::create(context, objectFields, "Clojure_Object");

  frameTy = StructType::create(context,
                               {ptrTy,                     // leafFrame
                                ptrTy,                     // method
                                RT_valueTy,                // self
                                RT_valueTy,                // variadicSeq
                                i32Ty,                     // bailoutEntryIndex
                                i32Ty,                     // localsCount
                                ArrayType::get(i64Ty, 0)}, // locals
                               "Clojure_Frame");

  methodTy = StructType::create(context,
                                {i8Ty,   // index
                                 i8Ty,   // isVariadic
                                 i8Ty,   // fixedArity
                                 i8Ty,   // closedOversCount
                                 ptrTy,  // baselineImplementation
                                 ptrTy,  // loopId
                                 ptrTy}, // closedOvers
                                "Clojure_FunctionMethod");

  clojureFunctionTy = StructType::create(context,
                                         {
                                             RT_objectTy,
                                             i8Ty,   // once
                                             i8Ty,   // executed
                                             wordTy, // methodCount
                                             wordTy, // maxArity
                                             ArrayType::get(methodTy, 0) // methods
                                         },
                                         "Clojure_Function");
  /* Usually 6 args fit in registers */
  baselineFunctionTy = FunctionType::get(
      RT_valueTy,
      {ptrTy, RT_valueTy, RT_valueTy, RT_valueTy, RT_valueTy, RT_valueTy},
      false);
}

llvm::Type *LLVMTypes::typeForType(const ObjectTypeSet &type) {
  if (type.isBoxedType())
    return i64Ty;
  if (type.isUnboxedPointer())
    return ptrTy;

  if (type.isUnboxedType(integerType))
    return i32Ty;
  if (type.isUnboxedType(doubleType))
    return doubleTy;
  if (type.isUnboxedType(keywordType))
    return i32Ty;
  if (type.isUnboxedType(symbolType))
    return i32Ty;
  if (type.isUnboxedType(booleanType))
    return i1Ty;

  throwInternalInconsistencyException("Type not supported: " + type.toString());

  return voidTy;
}

llvm::Value *LLVMTypes::getFrameLeafFramePtr(llvm::IRBuilder<> &builder,
                                            llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 0, "leafFrame_ptr");
}

llvm::Value *LLVMTypes::getFrameMethodPtr(llvm::IRBuilder<> &builder,
                                          llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 1, "method_ptr");
}

llvm::Value *LLVMTypes::getFrameSelfPtr(llvm::IRBuilder<> &builder,
                                        llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 2, "self_ptr");
}

llvm::Value *LLVMTypes::getFrameVariadicSeqPtr(llvm::IRBuilder<> &builder,
                                               llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 3, "variadicSeq_ptr");
}

llvm::Value *LLVMTypes::getFrameBailoutEntryIndexPtr(llvm::IRBuilder<> &builder,
                                                    llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 4, "bailoutEntryIndex_ptr");
}

llvm::Value *LLVMTypes::getFrameLocalsCountPtr(llvm::IRBuilder<> &builder,
                                               llvm::Value *framePtr) {
  return builder.CreateStructGEP(frameTy, framePtr, 5, "localsCount_ptr");
}

llvm::Value *LLVMTypes::getFrameLocalPtr(llvm::IRBuilder<> &builder,
                                         llvm::Value *framePtr,
                                         llvm::Value *index) {
  return builder.CreateInBoundsGEP(
      frameTy, framePtr, {builder.getInt32(0), builder.getInt32(6), index},
      "arg_ptr");
}

llvm::Value *LLVMTypes::getFrameLocalPtr(llvm::IRBuilder<> &builder,
                                         llvm::Value *framePtr,
                                         uint32_t index) {
  return builder.CreateInBoundsGEP(
      frameTy, framePtr,
      {builder.getInt32(0), builder.getInt32(6), builder.getInt32(index)},
      "arg_ptr");
}

llvm::Value *LLVMTypes::getMethodIsVariadicPtr(llvm::IRBuilder<> &builder,
                                               llvm::Value *methodPtr) {
  return builder.CreateStructGEP(methodTy, methodPtr, 1, "isVariadic_ptr");
}

llvm::Value *LLVMTypes::getMethodFixedArityPtr(llvm::IRBuilder<> &builder,
                                               llvm::Value *methodPtr) {
  return builder.CreateStructGEP(methodTy, methodPtr, 2, "fixedArity_ptr");
}

llvm::Value *LLVMTypes::getMethodImplementationPtr(llvm::IRBuilder<> &builder,
                                                   llvm::Value *methodPtr) {
  return builder.CreateStructGEP(methodTy, methodPtr, 4,
                                 "baselineImplementation_ptr");
}

llvm::Value *LLVMTypes::getMethodClosedOversPtr(llvm::IRBuilder<> &builder,
                                                llvm::Value *methodPtr) {
  return builder.CreateStructGEP(methodTy, methodPtr, 6, "closedOvers_ptr");
}

uint64_t LLVMTypes::getFunctionMethodsOffset(const llvm::DataLayout &DL) {
  return DL.getStructLayout(clojureFunctionTy)->getElementOffset(5);
}

} // namespace rt
