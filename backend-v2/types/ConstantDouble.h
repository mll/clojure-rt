#ifndef CONSTANT_DOUBLE_H
#define CONSTANT_DOUBLE_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantDouble: public ObjectTypeConstant {
  public:
  double value;
  ConstantDouble(double val) : ObjectTypeConstant(doubleType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantDouble(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantDouble *i = dynamic_cast<ConstantDouble *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
