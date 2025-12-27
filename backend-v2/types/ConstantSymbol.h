#ifndef CONSTANT_SYMBOL_H
#define CONSTANT_SYMBOL_H

#include "ObjectTypeConstant.h"

namespace rt {

class ConstantSymbol: public ObjectTypeConstant {
  public:
  std::string value;
  ConstantSymbol(std::string val) : ObjectTypeConstant(keywordType), value(val) {}
  virtual ObjectTypeConstant *copy() { return static_cast<ObjectTypeConstant *> (new ConstantSymbol(value)); }
  virtual std::string toString() { return value; }
  virtual bool equals(ObjectTypeConstant *other) {   
    if(ConstantSymbol *i = dynamic_cast<ConstantSymbol *>(other)) {
      return i->value == value;
    }
    return false;
  }
};

} // namespace rt

#endif
