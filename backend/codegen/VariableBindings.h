#ifndef VARIABLE_BINDINGS_H
#define VARIABLE_BINDINGS_H

#include "../RuntimeHeaders.h"
#include "../bridge/Exceptions.h"
#include "TypedValue.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

namespace rt {
template <typename T> class VariableBindings {
  std::vector<std::unordered_map<std::string, T>> variableBindingStack;

public:
  // Constructor
  VariableBindings() {}

  word_t stackDepth() const { return variableBindingStack.size(); }

  // level -1 means current level
  T *find(const std::string &key, word_t level = -1) {
    if (level != -1) {
      if (level >= stackDepth()) return nullptr;
      std::unordered_map<std::string, T> &levelMap = variableBindingStack[level];
      auto result = levelMap.find(key);
      if (result == levelMap.end())
        return nullptr;
      return &(result->second);
    }
    for (int i = (int)stackDepth() - 1; i >= 0; --i) {
      std::unordered_map<std::string, T> &levelMap = variableBindingStack[i];
      auto result = levelMap.find(key);
      if (result != levelMap.end())
        return &(result->second);
    }
    return nullptr;
  }

  // Always sets at the current level
  void set(const std::string &key, const T &value) {
    if (stackDepth() == 0)
      throwInternalInconsistencyException(
          "Internal error - the stack has no levels.");
    std::unordered_map<std::string, T> &levelMap =
        variableBindingStack[stackDepth() - 1];
    auto result = levelMap.find(key);
    if (result != levelMap.end())
      throwInternalInconsistencyException(
          "Internal error - variable: '" + key +
          "' is already present on variable stack.");
    levelMap[key] = value;
  }

  // Creates a new stack level and makes it current
  void push() {
    variableBindingStack.push_back(std::unordered_map<std::string, T>());
  }

  // Pops stack one level
  void pop() {
    if (stackDepth() == 0)
      throwInternalInconsistencyException(
          "Internal error - the stack has no levels.");
    variableBindingStack.pop_back();
  }
};

} // namespace rt

#endif // VALUE_ENCODER_H

/* struct LoopInsertionPoint { */
/*   llvm::BasicBlock *block; */
/*   llvm::PHINode *phiNode; */
/* };   */
/*   std::unordered_map<std::string, LoopInsertionPoint> loopInsertPoints; */
