
#include "ValueEncoder.h"
#include <llvm/IR/Constants.h>
#include "../runtime/word.h"
#include "../cljassert.h"
#include "TypedValue.h"

using namespace llvm;
using namespace rt;

ValueEncoder::ValueEncoder(LLVMContext& ctx, IRBuilder<>& b) 
    : context(ctx), builder(b), word_size(K_WORD_SIZE) {
    
    // 1. Cache standard types
    i64Ty = Type::getInt64Ty(context);
    i32Ty = Type::getInt32Ty(context);
    i1Ty  = Type::getInt1Ty(context);
    doubleTy = Type::getDoubleTy(context);
    ptrTy = PointerType::getUnqual(context); // Opaque pointer in LLVM 15+

    // 2. Define Tag Constants
    // Top 16 bits are used for tagging. 
    // Format: FFFF = Int, FFFE = Ptr, etc.
    TAG_MASK         = 0xFFFF000000000000ULL;
    TAG_DOUBLE_START = 0xFFF0000000000000ULL; 
    
    TAG_INT32 = 0xFFFF000000000000ULL;
    TAG_PTR   = 0xFFFE000000000000ULL;
    TAG_BOOL  = 0xFFFD000000000000ULL;
    TAG_NIL = 0xFFFC000000000000ULL;
    TAG_KEYWORD = 0xFFFB000000000000ULL;
    TAG_SYMBOL = 0xFFFA000000000000ULL;    
}

llvm::Value *ValueEncoder::u64(uint64_t val) {
    return ConstantInt::get(context, APInt(64, val));
}

// --- BOXING ---

TypedValue ValueEncoder::boxDouble(TypedValue doubleVal) {
  if(!doubleVal.type.isUnboxedType(doubleType)) return doubleVal;
  return TypedValue(
                    doubleVal.type.boxed(),
                    builder.CreateBitCast(doubleVal.value, i64Ty, "box_dbl"));
}

TypedValue ValueEncoder::boxInt32(TypedValue int32Val) {
  if(!int32Val.type.isUnboxedType(integerType)) return int32Val;
  llvm::Value *val64 = builder.CreateZExt(int32Val.value, i64Ty);
  return TypedValue(
                    int32Val.type.boxed(),
                    builder.CreateOr(val64, u64(TAG_INT32), "box_int"));
}

TypedValue ValueEncoder::boxBool(TypedValue boolVal) {
  if(!boolVal.type.isUnboxedType(booleanType)) return boolVal;
    llvm::Value *val64 = builder.CreateZExt(boolVal.value, i64Ty);
    return TypedValue(boolVal.type.boxed(),
                      builder.CreateOr(val64, u64(TAG_BOOL), "box_bool"));
}

TypedValue ValueEncoder::boxNil() {
    return TypedValue(ObjectTypeSet(nilType, true, new ConstantNil()),
                      u64(TAG_NIL));
}

TypedValue ValueEncoder::boxPointer(TypedValue rawPtr) {
  if(!rawPtr.type.isUnboxedPointer()) return rawPtr;
  // Cast Ptr -> Int. 
  // On 32-bit arch, this results in i32, so we must ZExt to i64.
  llvm::Value *ptrAsInt = builder.CreatePtrToInt(rawPtr.value, 
                                                 word_size == 64 ? i64Ty : i32Ty);
  
  if (word_size == 32) {
    ptrAsInt = builder.CreateZExt(ptrAsInt, i64Ty);
  }
  return TypedValue(rawPtr.type.boxed(),
                    builder.CreateOr(ptrAsInt, u64(TAG_PTR), "box_ptr"));
}

TypedValue ValueEncoder::boxKeyword(TypedValue keywordId) {
  if(!keywordId.type.isUnboxedType(keywordType)) return keywordId;
  // keywordId should be an i32 ID
  llvm::Value *val64 = builder.CreateZExt(keywordId.value, i64Ty);
  return TypedValue(keywordId.type.boxed(),
                    builder.CreateOr(val64, u64(TAG_KEYWORD), "box_kw"));
}

TypedValue ValueEncoder::boxSymbol(TypedValue symbolId) {
  if(!symbolId.type.isUnboxedType(symbolType)) return symbolId;
  // symbolId should be an i32 ID
  llvm::Value *val64 = builder.CreateZExt(symbolId.value, i64Ty);
  return TypedValue(symbolId.type.boxed(),
                    builder.CreateOr(val64, u64(TAG_SYMBOL), "box_kw"));
}

// --- TYPE CHECKING ---

TypedValue ValueEncoder::isDouble(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");
  if (boxedVal.type.isBoxedType(doubleType))
    return TypedValue(
        ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
        builder.getInt1(true));
  // Unsigned Less Than "Start of Tag Space" means it is a double
  return TypedValue(ObjectTypeSet(booleanType, false),
                    builder.CreateICmpULT(boxedVal.value, u64(TAG_DOUBLE_START), "is_dbl"));
}

TypedValue ValueEncoder::isInt32(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");  
  if (boxedVal.type.isBoxedType(integerType))
    return TypedValue(
        ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
        builder.getInt1(true));
    llvm::Value *masked = builder.CreateAnd(boxedVal.value, u64(TAG_MASK));
    return TypedValue(ObjectTypeSet(booleanType, false),
                      builder.CreateICmpEQ(masked, u64(TAG_INT32), "is_int"));
}

TypedValue ValueEncoder::isBool(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");      
  if (boxedVal.type.isBoxedType(booleanType))
    return TypedValue(
                      ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
                      builder.getInt1(true));
  llvm::Value *masked = builder.CreateAnd(boxedVal.value, u64(TAG_MASK));
  return TypedValue(ObjectTypeSet(booleanType, false),  
                    builder.CreateICmpEQ(masked, u64(TAG_BOOL), "is_bool"));
}

TypedValue ValueEncoder::isNil(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");  
  if (boxedVal.type.isBoxedType(nilType))
    return TypedValue(
                      ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
                      builder.getInt1(true));
  return TypedValue(ObjectTypeSet(booleanType, false),  
                    builder.CreateICmpEQ(boxedVal.value, u64(TAG_NIL), "is_null"));
}

TypedValue ValueEncoder::isPointer(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");    
  if (boxedVal.type.isBoxedPointer())
    return TypedValue(
                      ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
                      builder.getInt1(true));
  llvm::Value *masked = builder.CreateAnd(boxedVal.value, u64(TAG_MASK));
  return TypedValue(ObjectTypeSet(booleanType, false),
                    builder.CreateICmpEQ(masked, u64(TAG_PTR), "is_ptr"));
}

TypedValue ValueEncoder::isKeyword(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");
    if (boxedVal.type.isBoxedType(keywordType))
    return TypedValue(
                      ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
                      builder.getInt1(true));
  llvm::Value *masked = builder.CreateAnd(boxedVal.value, u64(TAG_MASK));
  return TypedValue(ObjectTypeSet(booleanType, false),
                    builder.CreateICmpEQ(masked, u64(TAG_KEYWORD), "is_kw"));
}

TypedValue ValueEncoder::isSymbol(TypedValue boxedVal) {
  CLJ_ASSERT(boxedVal.type.isBoxedType(), "Unboxed value passed to ValueEncoder");
  if (boxedVal.type.isBoxedType(symbolType))
    return TypedValue(
                      ObjectTypeSet(booleanType, false, new ConstantBoolean(true)),
                      builder.getInt1(true));
  llvm::Value *masked = builder.CreateAnd(boxedVal.value, u64(TAG_MASK));
  return TypedValue(ObjectTypeSet(booleanType, false),
                    builder.CreateICmpEQ(masked, u64(TAG_SYMBOL), "is_sym"));
}

// --- UNBOXING ---

TypedValue ValueEncoder::unboxDouble(TypedValue boxedVal) {
  if (boxedVal.type.isUnboxedType(doubleType)) return boxedVal;
  return TypedValue(ObjectTypeSet(doubleType, false),
                    builder.CreateBitCast(boxedVal.value, doubleTy, "unbox_dbl"));
}

TypedValue ValueEncoder::unboxInt32(TypedValue boxedVal) {
  if(boxedVal.type.isUnboxedType(integerType)) return boxedVal;
  return TypedValue(ObjectTypeSet(integerType, false),
                    builder.CreateTrunc(boxedVal.value, i32Ty, "unbox_int"));
}

TypedValue ValueEncoder::unboxBool(TypedValue boxedVal) {
  if(boxedVal.type.isUnboxedType(booleanType)) return boxedVal;
  return TypedValue(ObjectTypeSet(booleanType, false),
                    builder.CreateTrunc(boxedVal.value, i1Ty, "unbox_bool"));
}

TypedValue ValueEncoder::unboxPointer(TypedValue boxedVal) {
  if(boxedVal.type.isUnboxedPointer()) return boxedVal;      
  llvm::Value *cleanVal = builder.CreateAnd(boxedVal.value, u64(~TAG_MASK));
  
  // Cast back to Pointer
  if (word_size == 32) {
    llvm::Value *ptr32 = builder.CreateTrunc(cleanVal, i32Ty);
    return TypedValue(boxedVal.type.unboxed(), builder.CreateIntToPtr(ptr32, ptrTy, "unbox_ptr"));
  } else {
    return TypedValue(boxedVal.type.unboxed(), builder.CreateIntToPtr(cleanVal, ptrTy, "unbox_ptr"));
  }
}

TypedValue ValueEncoder::unboxKeyword(TypedValue boxedVal) {
  if(boxedVal.type.isBoxedType(keywordType)) return boxedVal;      
  // Strip the tag and return the ID (usually i32 is enough for enum IDs)
  return TypedValue(ObjectTypeSet(keywordType, false),
                    builder.CreateTrunc(boxedVal.value, i32Ty, "unbox_kw"));
}

TypedValue ValueEncoder::unboxSymbol(TypedValue boxedVal) {
  if(boxedVal.type.isBoxedType(symbolType)) return boxedVal;    
  // Strip the tag and return the ID (usually i32 is enough for enum IDs)
  return TypedValue(ObjectTypeSet(symbolType, false),
                    builder.CreateTrunc(boxedVal.value, i32Ty, "unbox_sym"));
}
