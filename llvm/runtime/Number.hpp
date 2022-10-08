#ifndef RT_NUMBER
#define RT_NUMBER
#include "Object.hpp"
 
class Number: public Object {
  public:
  int64_t value;
  Number(int64_t value);
  bool equals(Number *other);

  virtual String *toString() override;
  virtual uint64_t hash() override; 
  virtual bool equals(Object *other) override;
};

#endif
