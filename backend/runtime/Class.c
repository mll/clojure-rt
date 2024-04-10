#include "Object.h"
#include "Class.h"
#include <stdarg.h>

Class* Class_create(String *name, String *typeName, uint64_t fieldCount, ...) {
  va_list args;
  va_start(args, fieldCount);
  Object *super = allocate(sizeof(Object) + sizeof(Class) + fieldCount * sizeof(String *)); 
  Class *self = (Class *)(super + 1);
  self->name = name;
  self->className = typeName;
  self->fieldCount = fieldCount;
  for (int i = 0; i < fieldCount; ++i) {
    String *field = va_arg(args, String *);
    self->fields[i] = field;
  }
  va_end(args);
  Object_create(super, classType);
  return self;
}

/* outside refcount system */
BOOL Class_equals(Class *self, Class *other) {
  // Two classes with the same name and field names don't have to be equal
  return self == other; // Pointer comparison, COMPARE anything else?
}

/* outside refcount system */
uint64_t Class_hash(Class *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(hash(self->className), hash(self->name));
}

String *Class_toString(Class *self) {
  String *prefix = String_create("class ");
  retain(self->className);
  String *retVal = String_concat(prefix, self->className);
  release(self);
  return retVal;
}

void Class_destroy(Class *self) {
  release(self->className);
  release(self->name);
  for (int i = 0; i < self->fieldCount; ++i) release(self->fields[i]);
}
