#ifndef CONSTANT_STRING_SET
#define CONSTANT_STRING_SET

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantString: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantString(std::string val) : ObjectTypeConstant(stringType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantString(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantString *i = dynamic_cast<ConstantString *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
