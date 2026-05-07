#ifndef CONSTANT_BOOL_H
#define CONSTANT_BOOL_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantBoolean: public ObjectTypeConstant {
  public:
  bool value;
  ConstantBoolean(bool val) : ObjectTypeConstant(booleanType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantBoolean(value)); }
  virtual std::string toString() { return std::to_string(value); }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantBoolean *i = dynamic_cast<ConstantBoolean *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
