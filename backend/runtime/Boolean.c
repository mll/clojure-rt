#include "Boolean.h"
#include "String.h"
#include "Object.h"
#include "sds/sds.h"

Boolean* Boolean_create(BOOL initial) {
  Object *super = allocate(sizeof(Boolean) + sizeof(Object)); 
  Boolean *self = (Boolean *)(super + 1);
  self->value = initial;
  Object_create(super, booleanType);
  return self;
}

BOOL Boolean_equals(Boolean *self, Boolean *other) {
  BOOL retVal = self->value == other->value;
  release(self);
  release(other);
  return retVal;
}

uint64_t Boolean_hash(Boolean *self) {
  uint64_t retVal = combineHash(5381, ((uint64_t) self->value) + 1); 
  release(self);
  return retVal;
}

String *Boolean_toString(Boolean *self) {  
  String *retVal = String_create(self->value ? sdsnew("true") : sdsnew("false"));
  release(self);
  return retVal;
}

void Boolean_destroy(Boolean *self) {
}
