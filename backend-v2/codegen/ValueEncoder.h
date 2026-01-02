#ifndef VALUE_ENCODER_H
#define VALUE_ENCODER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include "TypedValue.h"
#include "LLVMTypes.h"

namespace rt {

/**
 * NaN boxing.
 */
  
  class ValueEncoder {
    llvm::LLVMContext& context;
    llvm::IRBuilder<> &builder;
    LLVMTypes &types;
    int word_size; // 32 or 64
    
    // Internal helper
    llvm::Value* u64(uint64_t val);
    
  public:
    // Tag Constants
    static constexpr uint64_t TAG_MASK = 0xFFFF000000000000ULL;
    static constexpr uint64_t TAG_DOUBLE_START = 0xFFF0000000000000ULL;
    static constexpr uint64_t TAG_INT32 = 0xFFFF000000000000ULL;
    static constexpr uint64_t TAG_PTR = 0xFFFE000000000000ULL;
    static constexpr uint64_t TAG_BOOL = 0xFFFD000000000000ULL;
    static constexpr uint64_t TAG_NIL = 0xFFFC000000000000ULL;
    static constexpr uint64_t TAG_KEYWORD = 0xFFFB000000000000ULL;
    static constexpr uint64_t TAG_SYMBOL = 0xFFFA000000000000ULL;    

    // Constructor
    explicit ValueEncoder(llvm::LLVMContext& ctx, llvm::IRBuilder<>& b, LLVMTypes &t);

    // Boxing (Creating safe 64-bit values)
    TypedValue box(TypedValue val);
    
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
    TypedValue unbox(TypedValue val);
    
    TypedValue unboxDouble(TypedValue boxedVal);
    TypedValue unboxInt32(TypedValue boxedVal);
    TypedValue unboxBool(TypedValue boxedVal);
    TypedValue unboxPointer(TypedValue boxedVal);
    TypedValue unboxKeyword(TypedValue boxedVal);
    TypedValue unboxSymbol(TypedValue boxedVal);    
};

} // namespace rt

#endif // VALUE_ENCODER_H
