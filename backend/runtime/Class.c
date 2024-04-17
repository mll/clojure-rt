#include "Object.h"
#include "Class.h"
#include "Hash.h"
#include <stdarg.h>

Class* Class_create(String *name, String *typeName, uint64_t fieldCount, ...) {
  va_list args;
  va_start(args, fieldCount);
  Object *super = allocate(sizeof(Object) + sizeof(Class) + fieldCount * sizeof(String *)); 
  Class *self = (Class *)(super + 1);
  self->registerId = 0; // unregistered
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
  // Unregistered classes should never appear here
  return self->registerId == other->registerId;
}

/* outside refcount system */
uint64_t Class_hash(Class *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(avalanche_64(self->registerId), hash(self->name));
}

String *Class_toString(Class *self) {
  String *retVal = self->className;
  retain(retVal);
  release(self);
  return retVal;
}

void Class_destroy(Class *self) {
  release(self->className);
  release(self->name);
  for (int i = 0; i < self->fieldCount; ++i) release(self->fields[i]);
}

int64_t Class_fieldIndex(Class *self, String *field) {
  for (int i = 0; i < self->fieldCount; ++i) {
    if (String_equals(field, self->fields[i])) {
      release(self);
      release(field);
      return i;
    }
  }
  release(self);
  release(field);
  return -1;
}
