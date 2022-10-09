#include "Integer.h"
#include "Object.h"
#include "String.h"

Integer* Integer_create(int64_t integer) {
  Integer *self = malloc(sizeof(Integer));
  self->value = integer;
  self->super = Object_create(integerType, self);;
  return self;
}

bool Integer_equals(Integer *self, Integer *other) {
  return self->value == other->value;
}

uint64_t Integer_hash(Integer *self) {
  return (uint64_t) self->value; 
}

String *Integer_toString(Integer *self) {  
  return String_create(sdsfromlonglong(self->value));
}

void Integer_destroy(Integer *self) {
}
