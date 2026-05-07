#ifndef RT_OBJECT_TYPE_CONSTANT
#define RT_OBJECT_TYPE_CONSTANT

#include "../runtime/defines.h"
#include "../runtime/ObjectProto.h"
#include <string>


namespace rt {

class ObjectTypeConstant {
  protected:
  ObjectTypeConstant(objectType type) : constantType(type) {}
  public:
  objectType constantType;
  virtual ~ObjectTypeConstant() {}
  virtual ObjectTypeConstant *copy() { return nullptr; }
  virtual std::string toString() { return ""; }
  virtual bool equals(ObjectTypeConstant *other) {return false; }
};

}

#endif
