#ifndef CONSTANT_INTEGER_H
#define CONSTANT_INTEGER_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantInteger: public ObjectTypeConstant {
  public:
  int32_t value;
  ConstantInteger(int32_t val) : ObjectTypeConstant(integerType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantInteger(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantInteger *i = dynamic_cast<ConstantInteger *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
