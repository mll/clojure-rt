#include "Object.h"
#include "Symbol.h"
#include "String.h"

extern ConcurrentHashMap *keywords;
extern Nil *UNIQUE_NIL;

Symbol* Symbol_allocate(String *string) {
  Object *super = allocate(sizeof(Symbol) + sizeof(Object)); 
  Symbol *self = (Symbol *)(super + 1);
  self->string = string;
  retain(string);
  Object_create(super, symbolType);
  return self;
}

Symbol* Symbol_create(String *string) {
  return Symbol_allocate(string);
}

BOOL Symbol_equals(Symbol *self, Symbol *other) {
  /* because we are uniquing / interning */
  return equals(self->string, other->string);
}

uint64_t Symbol_hash(Symbol *self) {
    return self->string->hash;
}

String *Symbol_toString(Symbol *self) {
  retain(self->string);
  return self->string;
}

void Symbol_destroy(Symbol *self) {
  release(self->string);
}



