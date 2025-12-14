#ifndef LLVM_PASSES_RETAINRELEASE
#define LLVM_PASSES_RETAINRELEASE

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"

// The new pass inherits from PassInfoMixin
class RetainReleasePass : public llvm::PassInfoMixin<RetainReleasePass> {
public:
  // The new entry point for the pass.
  // It takes the Function and the FunctionAnalysisManager as arguments.
  llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);

  // This is a required static method for the new pass manager to identify the pass.
  static llvm::StringRef name() { return "RetainReleasePass"; }
};

#endif

