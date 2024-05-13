#include "Nil.h"
#include "String.h"
#include <stdio.h>
#include "Object.h"

Nil *UNIQUE_NIL = NULL;
/* mem done */
Nil* Nil_create() {  
  retain(UNIQUE_NIL);
  return UNIQUE_NIL;
}

/* mem done */
Nil* Nil_allocate() {
  Object *super = allocate(sizeof(Nil) + sizeof(Object)); 
  Nil *self = (Nil *)(super + 1);
  Object_create(super, nilType);
  return self;
}

/* mem done */
void Nil_initialise() {  
  UNIQUE_NIL = Nil_allocate();
}

/* outside refcount system */
BOOL Nil_equals(Nil *self, Nil *other) {
  return FALSE;
}

/* outside refcount system */
uint64_t Nil_hash(Nil *self) {
  return combineHash(5381, 512);
}

/* mem done */
String *Nil_toString(Nil *self) {
  release(self);
  return String_create("nil");
}

/* outside refcount system */
Nil * const Nil_getUnique() {
    return UNIQUE_NIL;
}

/* outside refcount system */
void Nil_destroy(Nil *self) {
}
