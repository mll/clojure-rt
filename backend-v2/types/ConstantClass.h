#ifndef CONSTANT_CLASS_H
#define CONSTANT_CLASS_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantClass: public ObjectTypeConstant {
  public:
  uword_t value;
  ConstantClass(uword_t val) : ObjectTypeConstant(functionType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantClass(value)); }
  virtual std::string toString() { return std::string("class_") + std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {
    if(ConstantClass *i = dynamic_cast<ConstantClass *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
