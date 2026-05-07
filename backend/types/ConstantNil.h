#ifndef CONSTANT_NIL_H
#define CONSTANT_NIL_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantNil: public ObjectTypeConstant {
  public:
  ConstantNil() : ObjectTypeConstant(nilType) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantNil()); }
  virtual std::string toString() { return "nil"; }
  virtual bool equals(ObjectTypeConstant *other) {   
    return dynamic_cast<ConstantNil *>(other);
  }
};

} // namespace rt

#endif
