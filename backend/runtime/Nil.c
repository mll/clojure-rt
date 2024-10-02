#include "Nil.h"
#include "String.h"
#include <stdio.h>
#include <stdarg.h>
#include "Object.h"

Nil *UNIQUE_NIL = NULL;
/* mem done */
Nil* Nil_create() {  
  retain(UNIQUE_NIL);
  return UNIQUE_NIL;
}

Nil* Nil_create1(void * obj) {
  release(obj);
  return Nil_create();
}

Nil* Nil_create2(void * obj1, void * obj2) {
  release(obj1);
  release(obj2);
  return Nil_create();
}

Nil* Nil_createN(uint64_t objCount, ...) {
  va_list args;
  va_start(args, objCount);
  for(int i=0; i<objCount; i++) {
    void *obj = va_arg(args, void *);
    release(obj);
  }
  va_end(args);
  return Nil_create();
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
void Nil_destroy(Nil *self) {
}
