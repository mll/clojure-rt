#include "Number.hpp"

using namespace std;

Number::Number(int64_t value) : value(value), Object(numberType) {}
bool Number::equals(Number *other) {
    return other->value == value;
}
  
String *Number::toString() {
  return new String(to_string(value));
}

uint64_t Number::hash() {
  return value;
}
 
bool Number::equals(Object *other) { 
  if (other->type != numberType) return false;
  auto typedOther = dynamic_cast<Number *>(other);
  return equals(typedOther);
}

