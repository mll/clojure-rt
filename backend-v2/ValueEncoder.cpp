
#include "ValueEncoder.h"
#include <llvm/IR/Constants.h>
#include "word.h"

using namespace llvm;
using namespace rt;

ValueEncoder::ValueEncoder(LLVMContext& ctx, IRBuilder<>& b) 
    : context(ctx), builder(b), word_size(kWordSize) {
    
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
    TAG_NULL = 0xFFFC000000000000ULL;
    TAG_KEYWORD = 0xFFFB000000000000ULL;
    TAG_SYMBOL = 0xFFFA000000000000ULL;    
}

Value* ValueEncoder::u64(uint64_t val) {
    return ConstantInt::get(context, APInt(64, val));
}

// --- BOXING ---

Value* ValueEncoder::boxDouble(Value* doubleVal) {
    return builder.CreateBitCast(doubleVal, i64Ty, "box_dbl");
}

Value* ValueEncoder::boxInt32(Value* int32Val) {
    Value* val64 = builder.CreateZExt(int32Val, i64Ty);
    return builder.CreateOr(val64, u64(TAG_INT32), "box_int");
}

Value* ValueEncoder::boxBool(Value* boolVal) {
    Value* val64 = builder.CreateZExt(boolVal, i64Ty);
    return builder.CreateOr(val64, u64(TAG_BOOL), "box_bool");
}

Value* ValueEncoder::boxNull() {
    return u64(TAG_NULL);
}

Value* ValueEncoder::boxPointer(Value* rawPtr) {
    // Cast Ptr -> Int. 
    // On 32-bit arch, this results in i32, so we must ZExt to i64.
    Value* ptrAsInt = builder.CreatePtrToInt(rawPtr, 
        word_size == 64 ? i64Ty : i32Ty);
        
    if (word_size == 32) {
        ptrAsInt = builder.CreateZExt(ptrAsInt, i64Ty);
    }
    return builder.CreateOr(ptrAsInt, u64(TAG_PTR), "box_ptr");
}

Value* ValueEncoder::boxKeyword(Value* keywordId) {
    // keywordId should be an i32 or i64 ID
    Value* val64 = builder.CreateZExt(keywordId, i64Ty);
    return builder.CreateOr(val64, u64(TAG_KEYWORD), "box_kw");
}

Value* ValueEncoder::boxSymbol(Value* symbolId) {
    // symbolId should be an i32 or i64 ID
    Value* val64 = builder.CreateZExt(symbolId, i64Ty);
    return builder.CreateOr(val64, u64(TAG_SYMBOL), "box_kw");
}

// --- TYPE CHECKING ---

Value* ValueEncoder::isDouble(Value* boxedVal) {
    // Unsigned Less Than "Start of Tag Space" means it is a double
    return builder.CreateICmpULT(boxedVal, u64(TAG_DOUBLE_START), "is_dbl");
}

Value* ValueEncoder::isInt32(Value* boxedVal) {
    Value* masked = builder.CreateAnd(boxedVal, u64(TAG_MASK));
    return builder.CreateICmpEQ(masked, u64(TAG_INT32), "is_int");
}

Value* ValueEncoder::isBool(Value* boxedVal) {
    Value* masked = builder.CreateAnd(boxedVal, u64(TAG_MASK));
    return builder.CreateICmpEQ(masked, u64(TAG_BOOL), "is_bool");
}

Value* ValueEncoder::isNull(Value* boxedVal) {
    return builder.CreateICmpEQ(boxedVal, u64(TAG_NULL), "is_null");
}

Value* ValueEncoder::isPointer(Value* boxedVal) {
    Value* masked = builder.CreateAnd(boxedVal, u64(TAG_MASK));
    return builder.CreateICmpEQ(masked, u64(TAG_PTR), "is_ptr");
}

Value* ValueEncoder::isKeyword(Value* boxedVal) {
    Value* masked = builder.CreateAnd(boxedVal, u64(TAG_MASK));
    return builder.CreateICmpEQ(masked, u64(TAG_KEYWORD), "is_kw");
}

Value* ValueEncoder::isSymbol(Value* boxedVal) {
    Value* masked = builder.CreateAnd(boxedVal, u64(TAG_MASK));
    return builder.CreateICmpEQ(masked, u64(TAG_SYMBOL), "is_sym");
}

// --- UNBOXING ---

Value* ValueEncoder::unboxDouble(Value* boxedVal) {
    return builder.CreateBitCast(boxedVal, doubleTy, "unbox_dbl");
}

Value* ValueEncoder::unboxInt32(Value* boxedVal) {
    return builder.CreateTrunc(boxedVal, i32Ty, "unbox_int");
}

Value* ValueEncoder::unboxBool(Value* boxedVal) {
    return builder.CreateTrunc(boxedVal, i1Ty, "unbox_bool");
}

Value* ValueEncoder::unboxPointer(Value* boxedVal) {
    Value* cleanVal = builder.CreateAnd(boxedVal, u64(~TAG_MASK));
    
    // Cast back to Pointer
    if (word_size == 32) {
        Value* ptr32 = builder.CreateTrunc(cleanVal, i32Ty);
        return builder.CreateIntToPtr(ptr32, ptrTy, "unbox_ptr");
    } else {
        return builder.CreateIntToPtr(cleanVal, ptrTy, "unbox_ptr");
    }
}

Value* ValueEncoder::unboxKeyword(Value* boxedVal) {
    // Strip the tag and return the ID (usually i32 is enough for enum IDs)
    return builder.CreateTrunc(boxedVal, i32Ty, "unbox_kw");
}

Value* ValueEncoder::unboxSymbol(Value* boxedVal) {
    // Strip the tag and return the ID (usually i32 is enough for enum IDs)
    return builder.CreateTrunc(boxedVal, i32Ty, "unbox_sym");
}
