#include "RetainRelease.h"
#include "llvm/ADT/simple_ilist.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h" // For MergeBlockIntoPredecessor
#include <unordered_map>
#include <vector>


using namespace std;
using namespace llvm;

PreservedAnalyses RetainReleasePass::run(Function &F, FunctionAnalysisManager &AM) {
  bool isChanged = false;

  // The core logic remains the same, but we iterate using a for-loop for safety
  // with instruction/block deletion.
  for (BasicBlock &BB : F) {
    unordered_map<Value *, vector<Instruction *>> retains;
    // Iterate carefully, as we might erase instructions.
    for (auto instrIt = BB.begin(), E = BB.end(); instrIt != E; ) {
      Instruction &I = *instrIt;
      // Increment the iterator BEFORE any potential erasure.
      ++instrIt;

      if (MDNode* MD = I.getMetadata("memory_management")) {
        Value* val = nullptr;
        StringRef mmType = dyn_cast<MDString>(MD->getOperand(0))->getString();

        if (mmType == "retain") {
          if (CallBase* call = dyn_cast<CallBase>(&I)) {
            // It's better practice to check the argument count before accessing.
            if (call->arg_size() > 0) {
              val = call->getArgOperand(0);
            }
          } else if (AtomicRMWInst* inc = dyn_cast<AtomicRMWInst>(&I)) {
            val = inc->getPointerOperand();
          }

          if (val) {
            retains[val].push_back(&I);
          }
        } else if (mmType == "release") {
          if (CallBase* call = dyn_cast<CallBase>(&I)) {
            if (call->arg_size() > 0) {
              val = call->getArgOperand(0);
            }
          } else if (AtomicRMWInst* dec = dyn_cast<AtomicRMWInst>(&I)) {
            val = dec->getPointerOperand();
          }

          if (val) {
            auto it = retains.find(val);
            if (it != retains.end() && !it->second.empty()) {
              Instruction* retainToPair = it->second.back();
              it->second.pop_back();

              // Erase the paired retain and release instructions
              retainToPair->eraseFromParent();
              I.eraseFromParent();
              isChanged = true;
            }
          }
        }
      } else { // Any other instruction is a potential use
        if (User* user = dyn_cast<User>(&I)) {
          for (Value *operand : user->operands()) {
            auto it = retains.find(operand);
            // This logic assumes a use "consumes" a retain.
            if (it != retains.end() && !it->second.empty()) {
              it->second.pop_back();
            }
          }
        }
      }
    }
  }

  // If we changed the IR, we must return PreservedAnalyses::none().
  // Otherwise, we can claim we preserved all analyses.
  if (isChanged) {
    return PreservedAnalyses::none();
  }
  return PreservedAnalyses::all();
}
