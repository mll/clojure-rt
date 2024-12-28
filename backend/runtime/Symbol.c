#include "Object.h"
#include "Symbol.h"
#include "String.h"

extern ConcurrentHashMap *keywords;
extern Nil *UNIQUE_NIL;

/* mem done */
Symbol* Symbol_allocate(String *string) {
  // do we intern symbols? Doesnt seem so...
  Symbol *self = (Symbol *)allocate(sizeof(Symbol));
  self->string = string;
  Object_create((Object *)self, symbolType);
  return self;
}

/* mem done */
Symbol* Symbol_create(String *string) {
  return Symbol_allocate(string);
}

/* outside refcount system */
BOOL Symbol_equals(Symbol *self, Symbol *other) {
  /* because we are uniquing / interning */
  return equals(self->string, other->string);
}

/* outside refcount system */
uint64_t Symbol_hash(Symbol *self) {
    return self->string->hash;
}

/* mem done */
String *Symbol_toString(Symbol *self) {
  String *retVal = self->string;
  retain(retVal);
  release(self);
  return retVal;
}

/* outside refcount system */
void Symbol_destroy(Symbol *self) {
  release(self->string);
}



