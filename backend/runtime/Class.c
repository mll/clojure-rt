#include "Object.h"
#include "Class.h"
#include "Hash.h"
#include <stdarg.h>

static inline uint64_t inc_mod(uint64_t n, uint64_t mod) {
  return (n == mod - 1) ? 0 : ++n;
}

// CONSIDER: Enforce static fields hash redistribution?
Class* Class_create(
  String *name,
  String *className,
  uint64_t flags, // clojure.asm.Opcodes

  uint64_t staticFieldCount,
  Keyword **staticFieldNames, // pass ownership of pointer, array of length staticFieldCount
  Object **staticFields, // pass ownership of pointer, array of length staticFieldCount
  
  uint64_t staticMethodCount,
  Keyword **staticMethodNames, // pass ownership of pointer, array of length staticMethodCount
  Function **staticMethods, // pass ownership of pointer, array of length staticMethodCount

  // TODO: Verify: If class is an interface, it can't have fields  
  uint64_t fieldCount,
  Keyword **fieldNames, // function will free that pointer and create new to hold reordered fields
  
  uint64_t methodCount,
  Keyword **methodNames, // pass ownership of pointer, array of length methodCount
  // TODO: Does methods require verifying? 
  // * Each method must take at least one argument (this)
  // * No closed overs
  // * Anything else?
  Function **methods // pass ownership of pointer, array of length methodCount
) {
  Object *super = allocate(sizeof(Object) + sizeof(Class));
  Class *self = (Class *)(super + 1);
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;
  self->isInterface = (flags & 0x0200) > 0; // TODO: static/Class.cpp, pull it from clojure.asm.Opcodes
  
  self->staticFieldCount = staticFieldCount;
  self->staticFieldNames = staticFieldNames;
  self->staticFields = staticFields;
  
  self->methodCount = methodCount;
  self->methodNames = methodNames;
  self->methods = methods;
  
  // redistribute fields according to hash modulo to improve lookup time
  self->fieldCount = fieldCount;
  if (fieldCount) {
    self->indexPermutation = allocate(fieldCount * sizeof(uint64_t));
    self->fieldNames = allocate(fieldCount * sizeof(Keyword *));
    memset(self->fieldNames, 0, fieldCount * sizeof(Keyword *));
    for (uint64_t i = 0; i < fieldCount; ++i) {
      Keyword *field = fieldNames[i];
      uint64_t j = Keyword_hash(field) % fieldCount;
      while (self->fieldNames[j]) j = inc_mod(j, fieldCount);
      self->fieldNames[j] = field;
      self->indexPermutation[i] = j;
    }
    deallocate(fieldNames);
  } else {
    self->indexPermutation = NULL;
    self->fieldNames = NULL;
  }
  
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
  for (int i = 0; i < self->staticFieldCount; ++i) {
    release(self->staticFieldNames[i]);
    Object_release(self->staticFields[i]);
  }
  for (int i = 0; i < self->staticMethodCount; ++i) {
    release(self->staticMethodNames[i]);
    release(self->staticMethods[i]);
  }
  for (int i = 0; i < self->fieldCount; ++i) {
    release(self->fieldNames[i]);
  }
  for (int i = 0; i < self->methodCount; ++i) {
    release(self->methodNames[i]);
    release(self->methods[i]);
  }
  if (self->staticFieldNames) deallocate(self->staticFieldNames);
  if (self->staticFields) deallocate(self->staticFields);
  if (self->staticMethodNames) deallocate(self->staticMethodNames);
  if (self->staticMethods) deallocate(self->staticMethods);
  if (self->fieldNames) deallocate(self->fieldNames);
  if (self->methodNames) deallocate(self->methodNames);
  if (self->methods) deallocate(self->methods);
}

int64_t Class_fieldIndex(Class *self, Keyword *field) {
  uint64_t initialGuess = Keyword_hash(field) % self->fieldCount;
  if (equals(field, self->fieldNames[initialGuess])) {
    release(self);
    release(field);
    return initialGuess;
  }
  for (uint64_t i = inc_mod(initialGuess, self->fieldCount); i != initialGuess; i = inc_mod(i, self->fieldCount)) {
    if (equals(field, self->fieldNames[i])) {
      release(self);
      release(field);
      return i;
    }
  }
  release(self);
  release(field);
  return -1;
}

int64_t Class_staticFieldIndex(Class *self, Keyword *staticField) {
  uint64_t initialGuess = Keyword_hash(staticField) % self->staticFieldCount;
  if (equals(staticField, self->staticFieldNames[initialGuess])) {
    release(self);
    release(staticField);
    return initialGuess;
  }
  for (uint64_t i = inc_mod(initialGuess, self->staticFieldCount); i != initialGuess; i = inc_mod(i, self->staticFieldCount)) {
    if (equals(staticField, self->staticFieldNames[i])) {
      release(self);
      release(staticField);
      return i;
    }
  }
  release(self);
  release(staticField);
  return -1;
}

void *Class_setIndexedStaticField(Class *self, int64_t i, void *value) {
  if (i < 0 || i >= self->staticFieldCount) {
    release(self); release(value); return NULL; // unsafe index exception - field not found?
  }
  Object *oldValue = self->staticFields[i];
  self->staticFields[i] = super(value);
  Object_release(oldValue);
  release(self);
  return value;
}

void *Class_getIndexedStaticField(Class *self, int64_t i) {
  if (i < 0 || i >= self->staticFieldCount) {
    release(self); return NULL; // unsafe index exception - field not found?
  }
  void *retVal = Object_data(self->staticFields[i]);
  retain(retVal);
  release(self);
  return retVal;
}
