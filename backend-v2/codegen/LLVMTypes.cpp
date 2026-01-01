#include "LLVMTypes.h"
#include "../RuntimeHeaders.h"

using namespace llvm;

namespace rt {
  
  LLVMTypes::LLVMTypes(LLVMContext& context) {    
    i64Ty = Type::getInt64Ty(context);
    i32Ty = Type::getInt32Ty(context);
    i1Ty  = Type::getInt1Ty(context);
    doubleTy = Type::getDoubleTy(context);
    ptrTy = PointerType::getUnqual(context); // Opaque pointer in LLVM 15+
    wordTy = K_WORD_SIZE == 32 ? i32Ty : i64Ty;
  }
  
} // namespace rt

