#include "Object.h"
#include "Interface.h"
#include "Hash.h"
#include <stdarg.h>


Interface *Interface_create(String *name, String * className,
                            uword_t methodCount,
                            RTValue * methodNames, ClojureFunction * *methods) {
  Interface *self = allocate(sizeof(Interface));
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;
    
  self->methodCount = methodCount;
  self->methodNames = methodNames;
  self->methods = methods;
    
  Object_create((Object *)self, interfaceType);
  return self;
}

/* outside refcount system */
bool Interface_equals(Interface *self, Interface *other) {
  // Unregistered classes should never appear here
  return self->registerId == other->registerId;
}

/* outside refcount system */
uword_t Interface_hash(Interface *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(avalanche(self->registerId), Object_hash((Object *)self->name));
}

String *Interface_toString(Interface *self) {
  String *retVal = self->className;
  Ptr_retain(retVal);
  Ptr_release(self);
  return retVal;
}

void Interface_destroy(Interface *self) {
  Ptr_release(self->name);
  Ptr_release(self->className);
  
  for (uword_t i = 0; i < self->methodCount; ++i) {
    release(self->methodNames[i]);
    Ptr_release(self->methods[i]);
  }
  if (self->methodNames) deallocate(self->methodNames);
  if (self->methods) deallocate(self->methods);
}
