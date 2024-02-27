#ifndef LLVM_PASSES_RETAINRELEASE
#define LLVM_PASSES_RETAINRELEASE

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace llvm;

class RetainReleasePass : public FunctionPass {
  public:
  
  static char ID;
  
  explicit RetainReleasePass() : FunctionPass(ID) {};
  bool runOnFunction(Function &F) override;
};

FunctionPass *createRetainReleasePass();

#endif
