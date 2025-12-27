#ifndef CONSTANT_FUNCTION_H
#define CONSTANT_FUNCTION_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantFunction: public ObjectTypeConstant {
  public:
  uword_t value;
  ConstantFunction(uword_t val) : ObjectTypeConstant(functionType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantFunction(value)); }
  virtual std::string toString() { return std::string("fn_") + std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {
    if(ConstantFunction *i = dynamic_cast<ConstantFunction *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif



