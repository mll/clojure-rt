#include "Object.hpp"

using namespace std;

String::String(string value) : Object(stringType), value(move(value)) {}

String::~String() {}

bool String::equals(String *other) {
   if (this == other) return true;
   return this->value == other->value;
}

String *String::toString() {
  retain();
  return this;
}

uint64_t String::hash() {
  std::hash<string> hasher;
  return hasher(value);
}
 
bool String::equals(Object *other) {
  if (this == other) return true;
  if (other->type != stringType) return false;
  auto typedOther = dynamic_cast<String *>(other);
  return equals(typedOther);
}

uint64_t String::length() {
  return value.length();
}

