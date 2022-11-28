#include "Integer.h"
#include "String.h"
#include "Object.h"
#include <stdlib.h>
#include <stdio.h>
#include "Hash.h"

/* mem done */
Integer* Integer_create(int64_t integer) {
  Object *super = allocate(sizeof(Integer) + sizeof(Object)); 
  Integer *self = (Integer *)(super + 1);
  self->value = integer;
  Object_create(super, integerType);
  return self;
}

/* outside refcount system */
BOOL Integer_equals(Integer *self, Integer *other) {
  return self->value == other->value;
}

/* outside refcount system */
uint64_t Integer_hash(Integer *self) {
  return combineHash(5381 , avalanche_64(self->value)); 
}

/* mem done */
String *Integer_toString(Integer *self) { 
  String *retVal = String_createDynamic(21);
  retVal->count = snprintf(retVal->value, 20, "%lld", self->value);
  String_recomputeHash(retVal);
  release(self);
  return retVal;
}

/* outside refcount system */
void Integer_destroy(Integer *self) {
}
