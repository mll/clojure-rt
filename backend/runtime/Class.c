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
  
  // Interfaces don't have superclasses
  // Among concrete classes, only java.lang.Class does not have superclass
  // There should be no cyclic class hierarchy without pointer manipulation (eg. not from user code)
  Class *superclass,

  uint64_t staticFieldCount,
  Keyword **staticFieldNames, // pass ownership of pointer, array of length staticFieldCount
  Object **staticFields, // pass ownership of pointer, array of length staticFieldCount
  
  uint64_t staticMethodCount,
  Keyword **staticMethodNames, // pass ownership of pointer, array of length staticMethodCount
  ClojureFunction **staticMethods, // pass ownership of pointer, array of length staticMethodCount

  // TODO: Verify: If a class is an interface, it can't have fields  
  uint64_t fieldCount,
  Keyword **fieldNames, // function will free that pointer and create new to hold reordered fields
  
  // Only for method defined in this class
  uint64_t methodCount,
  Keyword **methodNames, // pass ownership of pointer, array of length methodCount
  ClojureFunction **methods, // pass ownership of pointer, array of length methodCount | TODO: If the class is an interface, should there be mockup impls?

  uint64_t implementedInterfacesCount,
  Class **implementedInterfaceClasses, // pass ownership of pointer, array of length implementedInterfacesCount
  // pass ownership of pointer, 2d array of length implementedInterfacesCount,
  // implementedInterfacesCount[i] has length implementedInterfaces[i]->methodCount
  ClojureFunction ***implementedInterfaces
) {
  Object *super = allocate(sizeof(Object) + sizeof(Class));
  Class *self = (Class *)(super + 1);
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;
  self->isInterface = (flags & 0x0200) > 0; // TODO: static/Class.cpp, pull it from clojure.asm.Opcodes
  
  self->superclass = superclass;
  
  self->staticFieldCount = staticFieldCount;
  self->staticFieldNames = staticFieldNames;
  self->staticFields = staticFields;
  
  self->staticMethodCount = staticMethodCount;
  self->staticMethodNames = staticMethodNames;
  self->staticMethods = staticMethods;
  
  self->methodCount = methodCount;
  self->methodNames = methodNames;
  self->methods = methods;
  
  self->implementedInterfacesCount = implementedInterfacesCount;
  self->implementedInterfaceClasses = implementedInterfaceClasses;
  self->implementedInterfaces = implementedInterfaces;
  
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
  release(self->name);
  release(self->className);
  if (self->superclass) release(self->superclass);
  
  for (int i = 0; i < self->staticFieldCount; ++i) {
    release(self->staticFieldNames[i]);
    Object_release(self->staticFields[i]);
  }
  if (self->staticFieldNames) deallocate(self->staticFieldNames);
  if (self->staticFields) deallocate(self->staticFields);
  
  for (int i = 0; i < self->staticMethodCount; ++i) {
    release(self->staticMethodNames[i]);
    release(self->staticMethods[i]);
  }
  if (self->staticMethodNames) deallocate(self->staticMethodNames);
  if (self->staticMethods) deallocate(self->staticMethods);
  
  for (int i = 0; i < self->fieldCount; ++i) {
    release(self->fieldNames[i]);
  }
  if (self->indexPermutation) deallocate(self->indexPermutation);
  if (self->fieldNames) deallocate(self->fieldNames);
  
  for (int i = 0; i < self->methodCount; ++i) {
    release(self->methodNames[i]);
    release(self->methods[i]);
  }
  if (self->methodNames) deallocate(self->methodNames);
  if (self->methods) deallocate(self->methods);
  
  for (int i = 0; i < self->implementedInterfacesCount; ++i) {
    for (int j = 0; j < self->implementedInterfaceClasses[i]->methodCount; ++j) {
      release(self->implementedInterfaces[i][j]);
    }
    if (self->implementedInterfaces[i]) deallocate(self->implementedInterfaces[i]);
    release(self->implementedInterfaceClasses[i]);
  }
  if (self->implementedInterfaceClasses) deallocate(self->implementedInterfaceClasses);
  if (self->implementedInterfaces) deallocate(self->implementedInterfaces);
}

int64_t Class_fieldIndex(Class *self, Keyword *field) {
  if (!self->fieldCount) {
    release(self);
    release(field);
    return -1;
  }
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

ClojureFunction *Class_resolveInstanceCall(Class *self, Keyword *name, uint64_t argCount) {
  // Class method
  ClojureFunction *retVal = NULL;
  for (uint64_t i = 0; i < self->methodCount; ++i) {
    if (Keyword_equals(name, self->methodNames[i])
        && Function_validCallWithArgCount(self->methods[i], argCount)) {
      retVal = self->methods[i];
      retain(retVal);
      release(self);
      release(name);
      return retVal;
    }
  }
  
  // Interface implementation
  for (uint64_t i = 0; i < self->implementedInterfacesCount; ++i) {
    for (uint64_t j = 0; j < self->implementedInterfaceClasses[i]->methodCount; ++j) {
      if (Keyword_equals(name, self->implementedInterfaceClasses[i]->methodNames[j])
          && Function_validCallWithArgCount(self->implementedInterfaces[i][j], argCount)) {
        retVal = self->implementedInterfaces[i][j];
        retain(retVal);
        release(self);
        release(name);
        return retVal;
      }
    }
  }
  
  // Resolve in superclass
  if (self->superclass) {
    retain(self->superclass);
    retVal = Class_resolveInstanceCall(self->superclass, name, argCount);
    release(self);
  } else {
    release(self);
    release(name);
  }
  return retVal;
}
