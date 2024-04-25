#include "Object.h"
#include "Class.h"
#include "Hash.h"
#include <stdarg.h>

static inline uint64_t inc_mod(uint64_t n, uint64_t mod) {
  return (n == mod - 1) ? 0 : ++n;
}

Class* Class_create(String *name, String *typeName, uint64_t fieldCount, ...) {
  va_list args;
  va_start(args, fieldCount);
  Object *super = allocate(sizeof(Object) + sizeof(Class) + sizeof(uint64_t *) + fieldCount * sizeof(Keyword *));
  Class *self = (Class *)(super + 1);
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = typeName;
  self->indexPermutation = fieldCount > 0 ? allocate(fieldCount * sizeof(uint64_t)) : NULL;
  self->fieldCount = fieldCount;
  memset(self->fields, 0, fieldCount * sizeof(Keyword *));
  for (uint64_t i = 0; i < fieldCount; ++i) {
    Keyword *field = va_arg(args, Keyword *);
    // redistribute fields according to hash modulo
    uint64_t j = Keyword_hash(field) % fieldCount;
    while (self->fields[j]) j = inc_mod(j, fieldCount);
    self->fields[j] = field;
    self->indexPermutation[i] = j;
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

int64_t Class_fieldIndex(Class *self, Keyword *field) {
  uint64_t initialGuess = Keyword_hash(field) % self->fieldCount;
  if (equals(field, self->fields[initialGuess])) {
    release(self);
    // release(field);
    return initialGuess;
  }
  for (uint64_t i = inc_mod(initialGuess, self->fieldCount); i != initialGuess; i = inc_mod(i, self->fieldCount)) {
    if (equals(field, self->fields[i])) {
      release(self);
      // release(field);
      return i;
    }
  }
  release(self);
  // release(field);
  return -1;
}
