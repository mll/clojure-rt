#include "Class.h"
#include "Hash.h"
#include "Object.h"
#include "word.h"
#include <stdarg.h>

Class *ClassLookup(const char *className, void *jitEngine);

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

bool Class_isInstanceClassName(const char *className, void *jitEngine,
                               RTValue instance) {
  // Throws when class cannot be found.
  Class *cls = ClassLookup(className, jitEngine);
  bool retVal = Class_isInstance(cls, instance);
  Ptr_release(cls);
  release(instance);
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
