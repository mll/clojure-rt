#ifndef CONSTANT_FUNCTION_H
#define CONSTANT_FUNCTION_H

#include "ObjectTypeConstant.h"
#include <llvm/IR/Function.h>
#include <vector>

namespace rt {

struct ConstantMethod {
  int fixedArity;
  bool isVariadic;
  llvm::Function *implementation;

  bool operator==(const ConstantMethod &other) const {
    return fixedArity == other.fixedArity && isVariadic == other.isVariadic &&
           implementation == other.implementation;
  }
};

class ConstantFunction : public ObjectTypeConstant {
public:
  std::vector<ConstantMethod> methods;
  bool once;

  ConstantFunction(const std::vector<ConstantMethod> &methods, bool once = false)
      : ObjectTypeConstant(functionType), methods(methods), once(once) {}

  virtual ObjectTypeConstant *copy() {
    return static_cast<ObjectTypeConstant *>(new ConstantFunction(methods, once));
  }

  virtual std::string toString() {
    std::string res = "fn[";
    if (once)
      res += "once,";
    for (size_t i = 0; i < methods.size(); ++i) {
      res += std::to_string(methods[i].fixedArity) +
             (methods[i].isVariadic ? "+" : "");
      if (methods[i].implementation) {
        res += ":" + methods[i].implementation->getName().str();
      }
      if (i < methods.size() - 1)
        res += ",";
    }
    res += "]";
    return res;
  }

  virtual bool equals(ObjectTypeConstant *other) {
    if (ConstantFunction *i = dynamic_cast<ConstantFunction *>(other)) {
      return i->once == once && i->methods == methods;
    }
    return false;
  }
};

} // namespace rt

#endif
