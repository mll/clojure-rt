#include "BridgedObject.h"
#include "Object.h"
#include "String.h"
#include <stdio.h>

BridgedObject *BridgedObject_create(void *contents,
                                    void (*destructor)(void *, void *),
                                    void *jitEngine) {
  BridgedObject *self = (BridgedObject *)allocate(sizeof(BridgedObject));
  self->contents = contents;
  self->destructor = destructor;
  self->jitEngine = jitEngine;
  Object_create((Object *)self, bridgedObjectType);
  return self;
}

bool BridgedObject_equals(BridgedObject *self, BridgedObject *other) {
  return self->contents == other->contents;
}

uword_t BridgedObject_hash(BridgedObject *self) {
  return avalanche((uword_t)self->contents);
}

String *BridgedObject_toString(BridgedObject *self) {
  char buf[64];
  snprintf(buf, sizeof(buf), "[BridgedObject @ %p]", self->contents);
  return String_create(buf);
}

void BridgedObject_destroy(BridgedObject *self) {
  if (self->destructor) {
    self->destructor(self->contents, self->jitEngine);
  }
}
