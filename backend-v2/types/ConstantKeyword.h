#ifndef CONSTANT_KEYWORD_H
#define CONSTANT_KEYWORD_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantKeyword: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantKeyword(std::string val) : ObjectTypeConstant(keywordType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantKeyword(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantKeyword *i = dynamic_cast<ConstantKeyword *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
