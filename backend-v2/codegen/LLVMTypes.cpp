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

  RT_objectTy = StructType::create(context,
                                   {
                                       wordTy, // atomicRefCount
                                       i32Ty,  // type (enum)
#ifdef USE_MEMORY_BANKS
                                       Type::getInt8Ty(Ctx) // bankId
#endif
                                   },
                                   "Clojure_Object");

  frameTy = StructType::create(context,
                               {ptrTy,                     // leafFrame
                                ptrTy,                     // method
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

} // namespace rt
