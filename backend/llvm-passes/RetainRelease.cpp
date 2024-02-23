#include "llvm/Pass.h"
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

bool RetainReleasePass::runOnFunction(Function &F) {
  bool isChanged = false;
  auto blockIt = F.begin();
  while (blockIt != F.end()) {
    unordered_map<Value *, vector<Instruction *>> retains;
    auto instrIt = blockIt->begin();
    while (instrIt != blockIt->end()) {
      if (MDNode* MDNode = instrIt->getMetadata("memory_management")) { // retain or release instruction
        StringRef mmType = dyn_cast<MDString>(MDNode->operands()[0])->getString();
        if (mmType.equals("retain")) {
          if (CallBase* call = dyn_cast<CallBase>(instrIt)) {
            assert(call->getCalledFunction()->getName().equals("retain"));
            auto arg = call->arg_begin();
            assert(arg != call->arg_end());
            if (Value* val = dyn_cast<Value>(arg)) {
              auto instructions = retains.find(val);
              // instrIt->print(errs());
              if (instructions == retains.end()) {
                retains.insert({val, {&(*instrIt)}});
              } else {
                instructions->second.push_back(&(*instrIt));
              }
            }
          }
          ++instrIt;
        } else if (mmType.equals("release")) {
          if (CallBase* call = dyn_cast<CallBase>(instrIt)) {
            assert(call->getCalledFunction()->getName().equals("release"));
            auto arg = call->arg_begin();
            assert(arg != call->arg_end());
            if (Value* val = dyn_cast<Value>(arg)) {
              auto instructions = retains.find(val);
              // instrIt->print(errs());
              if (instructions != retains.end()) {
                auto retains = &instructions->second;
                if (!retains->empty()) {
                  Instruction* lastRelease = retains->back();
                  retains->pop_back();
                  lastRelease->eraseFromParent();
                  instrIt = instrIt->eraseFromParent();
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
        } else {
          ++instrIt;
        }
      } else { // any other instruction
        if (User* user = dyn_cast<User>(instrIt)) {
          for (auto val = user->value_op_begin(); val != user->value_op_end(); ++val) {
            auto valRetains = retains.find(*val);
            if (valRetains != retains.end() && !valRetains->second.empty()) {
              isChanged = true;
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
