#include "Class.h"
#include "Hash.h"
#include "Object.h"
#include "word.h"
#include <stdarg.h>

Class *Class_createInterface(String *name, String *className,
                             int32_t extendsInterfaceCount,
                             Class **extendsInterfaces) {
  Class *retVal =
      Class_create(name, className, extendsInterfaceCount, extendsInterfaces);
  retVal->isInterface = true;
  return retVal;
}

Class *Class_create(String *name, String *className, int32_t superclassCount,
                    Class **superclasses) {

  Class *self = allocate(sizeof(Class));
  self->isInterface = false;
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;
  self->superclassCount = superclassCount;
  self->superclasses = superclasses;

  self->compilerExtension = NULL;
  self->compilerExtensionDestructor = NULL;
  Object_create((Object *)self, classType);
  return self;
}

/* outside refcount system */
bool Class_equals(Class *self, Class *other) {
  // Unregistered classes should never appear here
  return self->registerId == other->registerId;
}

/* outside refcount system */
int32_t
Class_hash(Class *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(avalanche(self->registerId),
                     Object_hash((Object *)self->name));
}

String *Class_toString(Class *self) {
  String *retVal = self->className;
  Ptr_retain(retVal);
  Ptr_release(self);
  return retVal;
}

void Class_destroy(Class *self) {
  Ptr_release(self->name);
  Ptr_release(self->className);
  for (int32_t i = 0; i < self->superclassCount; i++)
    Ptr_release(self->superclasses[i]);

  if (self->superclasses)
    deallocate(self->superclasses);

  if (self->compilerExtension && self->compilerExtensionDestructor) {
    self->compilerExtensionDestructor(self->compilerExtension);
  }
}

// Bridge declarations (implemented in C++ side)
__attribute__((weak)) int32_t ClassExtension_fieldIndex(void *ext,
                                                        RTValue field) {
  return -1;
}
__attribute__((weak)) int32_t
ClassExtension_staticFieldIndex(void *ext, RTValue staticField) {
  return -1;
}
__attribute__((weak)) RTValue ClassExtension_getIndexedStaticField(void *ext,
                                                                   int32_t i) {
  return RT_boxNil();
}
__attribute__((weak)) RTValue
ClassExtension_setIndexedStaticField(void *ext, int32_t i, RTValue value) {
  return value;
}
__attribute__((weak)) ClojureFunction *
ClassExtension_resolveInstanceCall(void *ext, RTValue name, int32_t argCount) {
  return NULL;
}

int32_t Class_fieldIndex(Class *self, RTValue field) {
  int32_t retVal = -1;
  if (self->compilerExtension) {
    retVal = ClassExtension_fieldIndex(self->compilerExtension, field);
  }
  Ptr_release(self);
  release(field);
  return retVal;
}

int32_t Class_staticFieldIndex(Class *self, RTValue staticField) {
  int32_t retVal = -1;
  if (self->compilerExtension) {
    retVal =
        ClassExtension_staticFieldIndex(self->compilerExtension, staticField);
  }
  Ptr_release(self);
  release(staticField);
  return retVal;
}

RTValue Class_setIndexedStaticField(Class *self, int32_t i, RTValue value) {
  RTValue retVal = value;
  if (self->compilerExtension) {
    retVal =
        ClassExtension_setIndexedStaticField(self->compilerExtension, i, value);
  }
  Ptr_release(self);
  return retVal;
}

RTValue Class_getIndexedStaticField(Class *self, int32_t i) {
  RTValue retVal = RT_boxNil();
  if (self->compilerExtension) {
    retVal = ClassExtension_getIndexedStaticField(self->compilerExtension, i);
  }
  Ptr_release(self);
  return retVal;
}

ClojureFunction *Class_resolveInstanceCall(Class *self, RTValue name,
                                           int32_t argCount) {
  ClojureFunction *retVal = NULL;

  if (self->compilerExtension) {
    retVal = ClassExtension_resolveInstanceCall(self->compilerExtension, name,
                                                argCount);
    if (retVal) {
      Ptr_retain(retVal);
      Ptr_release(self);
      release(name);
      return retVal;
    }
  }

  // Resolve in superclasses (still needed in C if extension doesn't handle
  // hierarchy)
  for (int32_t i = 0; i < self->superclassCount; i++) {
    Ptr_retain(self->superclasses[i]);
    retVal = Class_resolveInstanceCall(self->superclasses[i], name, argCount);
    if (retVal) {
      Ptr_release(self);
      return retVal;
    }
  }

  Ptr_release(self);
  release(name);

  /* NULL signals the function was not found. */
  return retVal;
}

/* outside refcount system */
/* TODO: This is not yet done for interfaces */
bool Class_isInstanceObjectType(Class *current, int32_t targetRegisterId) {
  if (current->registerId == targetRegisterId) {
    return true;
  }
  for (int i = 0; i < current->superclassCount; i++) {
    if (Class_isInstanceObjectType(current->superclasses[i],
                                   targetRegisterId)) {
      return true;
    }
  }
  return false;
}
/* outside refcount system */
bool Class_isInstance(Class *current, RTValue instance) {
  objectType type = getType(instance);
  return Class_isInstanceObjectType(current, type);
}
