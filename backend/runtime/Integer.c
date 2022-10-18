#include "Integer.h"
#include "String.h"
#include "Object.h"
#include "sds/sds.h"

Integer* Integer_create(int64_t integer) {
  Object *super = allocate(sizeof(Integer) + sizeof(Object)); 
  Integer *self = (Integer *)(super + 1);
  self->value = integer;
  Object_create(super, integerType);
  return self;
}

BOOL Integer_equals(Integer *self, Integer *other) {
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
