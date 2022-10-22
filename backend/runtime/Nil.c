#include "Nil.h"
#include "String.h"
#include <stdio.h>
#include "Object.h"
#include "sds/sds.h"

Nil *UNIQUE_NIL = NULL;

Nil* Nil_create() {  
  retain(UNIQUE_NIL);
  return UNIQUE_NIL;
}

Nil* Nil_allocate() {
  Object *super = allocate(sizeof(Nil) + sizeof(Object)); 
  Nil *self = (Nil *)(super + 1);
  Object_create(super, nilType);
  return self;
}

void Nil_initialise() {  
  UNIQUE_NIL = Nil_allocate();
}

BOOL Nil_equals(Nil *self, Nil *other) {
  return TRUE;
}

uint64_t Nil_hash(Nil *self) {
  return 0;
}

String *Nil_toString(Nil *self) {
  return String_create(sdsnew("nil"));
}

void Double_destroy(Double *self) {
}
