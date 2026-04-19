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
  llvm::StructType *clojureFunctionTy;
  llvm::FunctionType *baselineFunctionTy;

  explicit LLVMTypes(llvm::LLVMContext &ctx);

  llvm::Type *typeForType(const ObjectTypeSet &type);

  // Frame accessors
  llvm::Value *getFrameLeafFramePtr(llvm::IRBuilder<> &builder,
                                   llvm::Value *framePtr);
  llvm::Value *getFrameMethodPtr(llvm::IRBuilder<> &builder,
                                 llvm::Value *framePtr);
  llvm::Value *getFrameSelfPtr(llvm::IRBuilder<> &builder,
                               llvm::Value *framePtr);
  llvm::Value *getFrameVariadicSeqPtr(llvm::IRBuilder<> &builder,
                                     llvm::Value *framePtr);
  llvm::Value *getFrameBailoutEntryIndexPtr(llvm::IRBuilder<> &builder,
                                           llvm::Value *framePtr);
  llvm::Value *getFrameLocalsCountPtr(llvm::IRBuilder<> &builder,
                                     llvm::Value *framePtr);
  llvm::Value *getFrameLocalPtr(llvm::IRBuilder<> &builder,
                                llvm::Value *framePtr, llvm::Value *index);
  llvm::Value *getFrameLocalPtr(llvm::IRBuilder<> &builder,
                                llvm::Value *framePtr, uint32_t index);

  // Method accessors
  llvm::Value *getMethodIsVariadicPtr(llvm::IRBuilder<> &builder,
                                      llvm::Value *methodPtr);
  llvm::Value *getMethodFixedArityPtr(llvm::IRBuilder<> &builder,
                                      llvm::Value *methodPtr);
  llvm::Value *getMethodImplementationPtr(llvm::IRBuilder<> &builder,
                                          llvm::Value *methodPtr);
  llvm::Value *getMethodClosedOversPtr(llvm::IRBuilder<> &builder,
                                       llvm::Value *methodPtr);

  // ClojureFunction accessors
  uint64_t getFunctionMethodsOffset(const llvm::DataLayout &DL);
};

} // namespace rt

#endif // LLVM_TYPES_H
