#include "llvm/Pass.h"
#include "llvm/ADT/simple_ilist.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "RetainRelease.h"
#include <unordered_map>

using namespace std;
using namespace llvm;

char RetainReleasePass::ID = 0;

FunctionPass *createRetainReleasePass() {
  return new RetainReleasePass();
};

// void disf(Function* x) {
//   errs() << "---> Function body:\n" << *x << "\n";
// }

// void disb(BasicBlock* x) {
//   errs() << "---> Basic block:\n" << *x << "\n";
// }

// void disi(Instruction* x) {
//   errs() << "---> Instruction:\n" << *x << "\n";
// }

bool RetainReleasePass::runOnFunction(Function &F) {
  bool isChanged = false;
  auto blockIt = F.begin();
  // errs() << "---> Function body:\n" << F << "\n";
  while (blockIt != F.end()) {
    // errs() << "---> Basic block:\n" << *blockIt << "\n";
    unordered_map<Value *, vector<Instruction *>> retains;
    auto instrIt = blockIt->begin();
    while (instrIt != blockIt->end()) {
      // errs() << "---> Instruction:\n" << *instrIt << "\n";
      if (MDNode* MDNode = instrIt->getMetadata("memory_management")) { // retain or release instruction
        Value* val = nullptr;
        StringRef mmType = dyn_cast<MDString>(MDNode->operands()[0])->getString();
        if (mmType.equals("retain")) {
          if (CallBase* call = dyn_cast<CallBase>(instrIt)) {
            assert(call->getCalledFunction()->getName().equals("retain"));
            auto arg = call->arg_begin();
            assert(arg != call->arg_end());
            val = dyn_cast<Value>(arg);
          } else if (AtomicRMWInst* inc = dyn_cast<AtomicRMWInst>(instrIt)) {
            val = inc->getPointerOperand();
          }
          if (val) {
            auto instructions = retains.find(val);
            if (instructions == retains.end()) {
              retains.insert({val, {&(*instrIt)}});
            } else {
              instructions->second.push_back(&(*instrIt));
            }
          }
          ++instrIt;
        } else if (mmType.equals("release")) {
          CallBase* call = dyn_cast<CallBase>(instrIt);
          AtomicRMWInst* dec = dyn_cast<AtomicRMWInst>(instrIt);
          if (call) {
            assert(call->getCalledFunction()->getName().equals("release"));
            auto arg = call->arg_begin();
            assert(arg != call->arg_end());
            val = dyn_cast<Value>(arg);
          } else if (dec) {
            val = dec->getPointerOperand();
          }
          if (val) {
            auto instructions = retains.find(val);
            if (instructions != retains.end()) {
              auto retains = &instructions->second;
              if (!retains->empty()) {
                Instruction* lastRelease = retains->back();
                retains->pop_back();
                lastRelease->eraseFromParent();
                isChanged = true;
                if (call) {
                  instrIt = instrIt->eraseFromParent(); // call release
                } else if (dec) {
                  // current_block:
                  //   ...
                  //   %0 = atomicrmw volatile sub ... <- instrIt
                  //   %cmp_jj = icmp eq i64 %0, 1
                  //   br i1 %cmp_jj, label %destroy, label %ignore
                  //
                  // destroy:
                  //   tail call void @Object_destroy ...
                  //   label %release_cont
                  //
                  // ignore:
                  //   label %release_cont
                  //
                  // release_cont:
                  //   %release_phi = phi i1 [ true, %destroy ], [ false, %ignore ] <- dummy value, not used
                  //   No other phi instructions!
                  // Delete remaining instructions in current block, starting from instrIt
                  // Delete destroy and ignore blocks
                  // Merge current_block (with deleted instructions) and release_cont (without release_phi)
                  
                  instrIt = instrIt->eraseFromParent(); // sub
                  assert(instrIt != blockIt->end() && "Expected icmp instruction, got end of block instead!");
                  instrIt = instrIt->eraseFromParent(); // icmp
                  assert(instrIt != blockIt->end() && "Expected br instruction, got end of block instead!");
                  BranchInst* br = dyn_cast<BranchInst>(instrIt);
                  assert(br && br->getNumSuccessors() == 2);
                  BasicBlock *destroy = br->getSuccessor(0),
                              *ignore = br->getSuccessor(1),
                              *cont = dyn_cast<BranchInst>(ignore->begin())->getSuccessor(0);
                  auto contIt = cont->getFirstNonPHIOrDbgOrAlloca();
                  blockIt->splice(blockIt->end(), cont, contIt, cont->end());
                  instrIt->eraseFromParent();
                  destroy->eraseFromParent();
                  ignore->eraseFromParent();
                  cont->eraseFromParent();
                  instrIt = contIt;
                }
              } else {
                ++instrIt;
              }
            } else {
              ++instrIt;
            }
          } else {
            ++instrIt;
          }
        } else {
          ++instrIt;
        }
      } else { // any other instruction
        if (User* user = dyn_cast<User>(instrIt)) {
          for (auto val = user->value_op_begin(); val != user->value_op_end(); ++val) {
            auto valRetains = retains.find(*val);
            if (valRetains != retains.end() && !valRetains->second.empty()) {
              valRetains->second.pop_back();
            }
          }
        }
        ++instrIt;
      }
    }
    ++blockIt;
  }
  return isChanged;
}
