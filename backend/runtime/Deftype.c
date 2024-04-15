#include "Object.h"
#include "Class.h"
#include "Deftype.h"
#include <stdarg.h>

Deftype* Deftype_create(Class *_class, uint64_t fieldCount, ...) {
  assert(fieldCount == _class->fieldCount);
  Object *superObject = allocate(sizeof(Object) + sizeof(Deftype) + fieldCount * sizeof(Object *)); 
  Deftype *self = (Deftype *)(superObject + 1);
  self->_class = _class;
  va_list args;
  va_start(args, fieldCount);
  for (int i = 0; i < fieldCount; ++i) {
    void *field = va_arg(args, void *);
    self->values[i] = super(field);
  }
  va_end(args);
  Object_create(superObject, deftypeType);
  return self;
}

/* outside refcount system */
BOOL Deftype_equals(Deftype *self, Deftype *other) {
  if (!equals(self->_class, other->_class)) return FALSE;
  for (int i = 0; i < self->_class->fieldCount; ++i)
    if (!Object_equals(self->values[i], other->values[i]))
      return FALSE;
  return TRUE;
}

/* outside refcount system */
uint64_t Deftype_hash(Deftype *self) {
  uint64_t initialHash = hash(self->_class);
  for (int i = 0; i < self->_class->fieldCount; ++i)
    initialHash = combineHash(initialHash, hash(self->values[i]));
  return initialHash;
}

// #object[ClassName 0x75d543f3 "ClassName@75d543f3"]
String *Deftype_toString(Deftype *self) {
  String *retVal = String_create("#object[");
  retain(self->_class->className);
  retVal = String_concat(retVal, self->_class->className);
  retVal = String_concat(retVal, String_create(" 0x"));
  char *raw_address = allocate(sizeof(char) * 21);
  snprintf(raw_address, 20, "%llx", (uint64_t) self); // address in hex, without 0x prefix
  String *address = String_create(raw_address);
  retain(address);
  retVal = String_concat(retVal, address);
  retVal = String_concat(retVal, String_create(" \""));
  retain(self->_class->className);
  retVal = String_concat(retVal, self->_class->className);
  retVal = String_concat(retVal, String_create("@"));
  retVal = String_concat(retVal, address);
  retVal = String_concat(retVal, String_create("\"]"));
  release(self);
  return retVal;
}

void Deftype_destroy(Deftype *self) {
  for (int i = 0; i < self->_class->fieldCount; ++i) Object_release(self->values[i]);
  release(self->_class);
}

void *Deftype_getField(Deftype *self, String *field) {
  uint64_t i = 0;
  while (i < self->_class->fieldCount) {
    if (equals(self->_class->fields[i], field)) break;
    ++i;
  }
  if (i == self->_class->fieldCount) {release(self); return NULL;} // field not found exception
  void *retVal = Object_data(self->values[i]);
  retain(retVal);
  release(self);
  return retVal;
}

Class *Deftype_getClass(Deftype *self) {
  Class *_class = self->_class;
  retain(_class);
  release(self);
  return _class;
}

void *Deftype_getIndexedField(Deftype *self, int64_t i) {
  if (i < 0 || i >= self->_class->fieldCount) {
    release(self); return NULL; // unsafe index exception - field not found?
  }
  void *retVal = Object_data(self->values[i]);
  retain(retVal);
  release(self);
  return retVal;
}
