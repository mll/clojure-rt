#ifndef VALUE_ENCODER_H
#define VALUE_ENCODER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include "TypedValue.h"

namespace rt {

/**
 * NaN boxing.
 */

class ValueEncoder {
private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;
    int word_size; // 32 or 64

    // LLVM Types (Cached for speed)
    llvm::Type* i64Ty;
    llvm::Type* i32Ty;
    llvm::Type* i1Ty;
    llvm::Type* doubleTy;
    llvm::Type* ptrTy;

    // Tag Constants
    uint64_t TAG_MASK;
    uint64_t TAG_DOUBLE_START;
    uint64_t TAG_INT32;
    uint64_t TAG_PTR;
    uint64_t TAG_BOOL;
    uint64_t TAG_NIL;
    uint64_t TAG_KEYWORD;
    uint64_t TAG_SYMBOL;    

    // Internal helper
    llvm::Value* u64(uint64_t val);

public:
    // Constructor
    explicit ValueEncoder(llvm::LLVMContext& ctx, llvm::IRBuilder<>& b);

    // Boxing (Creating safe 64-bit values)
    TypedValue boxDouble(TypedValue doubleVal);
    TypedValue boxInt32(TypedValue int32Val);
    TypedValue boxBool(TypedValue boolVal);
    TypedValue boxNil();
    TypedValue boxPointer(TypedValue rawPtr);
    TypedValue boxKeyword(TypedValue keywordId);
    TypedValue boxSymbol(TypedValue symbolId);    

    // Type Checking (Returns i1 boolean)
    TypedValue isDouble(TypedValue boxedVal);
    TypedValue isInt32(TypedValue boxedVal);
    TypedValue isBool(TypedValue boxedVal);
    TypedValue isNil(TypedValue boxedVal);
    TypedValue isPointer(TypedValue boxedVal);
    TypedValue isKeyword(TypedValue boxedVal);
    TypedValue isSymbol(TypedValue boxedVal);    

    // Unboxing (Returns raw type)
    TypedValue unboxDouble(TypedValue boxedVal);
    TypedValue unboxInt32(TypedValue boxedVal);
    TypedValue unboxBool(TypedValue boxedVal);
    TypedValue unboxPointer(TypedValue boxedVal);
    TypedValue unboxKeyword(TypedValue boxedVal);
    TypedValue unboxSymbol(TypedValue boxedVal);    
};

} // namespace rt

#endif // VALUE_ENCODER_H
