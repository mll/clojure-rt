#include "Object.h"
#include "Symbol.h"
#include "String.h"
#include "sds/sds.h"

Symbol* Symbol_create(String *name, String *namespace) {
  Object *super = allocate(sizeof(Symbol) + sizeof(Object)); 
  Symbol *self = (Symbol *)(super + 1);
  self->name = name;
  self->namespace = namespace;
  Object_create(super, symbolType);
  return self;
}


BOOL Symbol_equals(Symbol *self, Symbol *other) {
  return equals(super(self->name), super(other->name)) && equals(super(self->namespace), super(other->namespace));
}

uint64_t Symbol_hash(Symbol *self) {
  return combineHash(String_hash(self->name), String_hash(self->namespace)); 
}

String *Symbol_toString(Symbol *self) {
  sds new = sdsnew(self->namespace->value);
  new = sdscat(new, "/");   
  new = sdscat(new, self->name->value);
  return String_create(new);
}

void Symbol_destroy(Symbol *self) {
  release(self->name);
  release(self->namespace);
}


