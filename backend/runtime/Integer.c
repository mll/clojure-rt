#include "Integer.h"
#include "String.h"
#include "Object.h"
#include <stdlib.h>
#include <stdio.h>
#include "Hash.h"

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
  return combineHash(5381 , avalanche_64(self->value)); 
}

String *Integer_toString(Integer *self) { 
  String *retVal = String_createDynamic(21);
  retVal->count = snprintf(retVal->value, 20, "%lld", self->value);
  String_recomputeHash(retVal);
  return retVal;
}

void Integer_destroy(Integer *self) {
}
