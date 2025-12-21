#ifndef VALUE_ENCODER_H
#define VALUE_ENCODER_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>


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
    uint64_t TAG_NULL;
    uint64_t TAG_KEYWORD;
    uint64_t TAG_SYMBOL;    

    // Internal helper
    llvm::Value* u64(uint64_t val);

public:
    // Constructor
    ValueEncoder(llvm::LLVMContext& ctx, llvm::IRBuilder<>& b);

    // Boxing (Creating safe 64-bit values)
    llvm::Value* boxDouble(llvm::Value* doubleVal);
    llvm::Value* boxInt32(llvm::Value* int32Val);
    llvm::Value* boxBool(llvm::Value* boolVal);
    llvm::Value* boxNull();
    llvm::Value* boxPointer(llvm::Value *rawPtr);
    llvm::Value* boxKeyword(llvm::Value *keywordId);
    llvm::Value* boxSymbol(llvm::Value* symbolId);    

    // Type Checking (Returns i1 boolean)
    llvm::Value* isDouble(llvm::Value* boxedVal);
    llvm::Value* isInt32(llvm::Value* boxedVal);
    llvm::Value* isBool(llvm::Value* boxedVal);
    llvm::Value* isNull(llvm::Value* boxedVal);
    llvm::Value* isPointer(llvm::Value *boxedVal);
    llvm::Value* isKeyword(llvm::Value *boxedVal);
    llvm::Value* isSymbol(llvm::Value* boxedVal);    

    // Unboxing (Returns raw type)
    llvm::Value* unboxDouble(llvm::Value* boxedVal);
    llvm::Value* unboxInt32(llvm::Value* boxedVal);
    llvm::Value* unboxBool(llvm::Value* boxedVal);
    llvm::Value* unboxPointer(llvm::Value *boxedVal);
    llvm::Value* unboxKeyword(llvm::Value *boxedVal);
    llvm::Value* unboxSymbol(llvm::Value* boxedVal);    
};

} // namespace rt

#endif // VALUE_ENCODER_H
