#ifndef LLVM_TYPES_H
#define LLVM_TYPES_H

#include "../types/ObjectTypeSet.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

namespace rt {

struct LLVMTypes {
  // LLVM Types (Cached for speed)
  llvm::Type *wordTy;
  llvm::Type *i64Ty;
  llvm::Type *i32Ty;
  llvm::Type *i8Ty;
  llvm::Type *i1Ty;
  llvm::Type *doubleTy;
  llvm::Type *ptrTy;
  llvm::Type *voidTy;

  llvm::Type *RT_valueTy;
  llvm::StructType *RT_objectTy;
  llvm::StructType *frameTy;
  llvm::StructType *methodTy;
  llvm::FunctionType *baselineFunctionTy;

  explicit LLVMTypes(llvm::LLVMContext &ctx);

  llvm::Type *typeForType(const ObjectTypeSet &type);
};

} // namespace rt

#endif // LLVM_TYPES_H
