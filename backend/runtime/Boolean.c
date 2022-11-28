#include "Boolean.h"
#include "String.h"
#include "Object.h"

/* mem done */
Boolean* Boolean_create(BOOL initial) {
  Object *super = allocate(sizeof(Boolean) + sizeof(Object)); 
  Boolean *self = (Boolean *)(super + 1);
  self->value = initial;
  Object_create(super, booleanType);
  return self;
}

/* outside refcount system */
BOOL Boolean_equals(Boolean *self, Boolean *other) {
  return self->value == other->value;
}

/* outside refcount system */
uint64_t Boolean_hash(Boolean *self) {
  return combineHash(5381, ((uint64_t) self->value) + 5381); 
}

/* mem done */
String *Boolean_toString(Boolean *self) {  
  String *retVal = self->value ? String_create("true") : String_create("false");
  release(self);
  return retVal;
}

/* outside refcount system */
void Boolean_destroy(Boolean *self) {
}
