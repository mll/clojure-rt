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
  return self->value == other->value;
}

uint64_t Boolean_hash(Boolean *self) {
  return (uint64_t) self->value; 
}

String *Boolean_toString(Boolean *self) {  
  return String_create(self->value ? sdsnew("true") : sdsnew("false"));
}

void Boolean_destroy(Boolean *self) {
}
