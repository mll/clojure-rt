#ifndef CONSTANT_INTERFACE_H
#define CONSTANT_INTERFACE_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantInterface: public ObjectTypeConstant {
  public:
  uword_t value;
  ConstantInterface(uword_t val) : ObjectTypeConstant(functionType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantInterface(value)); }
  virtual std::string toString() { return std::string("ifc_") + std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {
    if(ConstantInterface *i = dynamic_cast<ConstantInterface *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
