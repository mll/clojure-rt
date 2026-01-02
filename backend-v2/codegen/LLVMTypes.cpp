#include "LLVMTypes.h"
#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"

using namespace llvm;

namespace rt {
  
  LLVMTypes::LLVMTypes(LLVMContext& context) {    
    i64Ty = Type::getInt64Ty(context);
    i32Ty = Type::getInt32Ty(context);
    i1Ty  = Type::getInt1Ty(context);
    doubleTy = Type::getDoubleTy(context);
    ptrTy = PointerType::getUnqual(context); // Opaque pointer in LLVM 15+
    wordTy = K_WORD_SIZE == 32 ? i32Ty : i64Ty;
    voidTy = Type::getVoidTy(context);
    RT_valueTy = i64Ty;

    RT_objectTy = StructType::create(context,
                                  {
                                      wordTy,      // atomicRefCount
                                      i32Ty,       // type (enum)
#ifdef USE_MEMORY_BANKS
                                      Type::getInt8Ty(Ctx) // bankId
#endif
                                  },
                                  "Clojure_Object");
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

    throwInternalInconsistencyException("Type not supported: " +
                                        type.toString());
    
    return voidTy;
  }    
  
} // namespace rt

