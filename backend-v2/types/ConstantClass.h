#ifndef CONSTANT_CLASS_H
#define CONSTANT_CLASS_H

#include "ObjectTypeConstant.h"
#include <string>

namespace rt {

class ConstantClass : public ObjectTypeConstant {
public:
  std::string value;
  ConstantClass(std::string val)
      : ObjectTypeConstant(classType), value(val) {}
  virtual ObjectTypeConstant *copy() {
    return static_cast<ObjectTypeConstant *>(new ConstantClass(value));
  }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {
    if (ConstantClass *i = dynamic_cast<ConstantClass *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
