#include "Symbol.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"

/* mem done - consumes string */
Symbol *Symbol_create(String *string) {
  Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
  Object_create(&sym->header, symbolType);
  sym->name = string;
  sym->ns = NULL;
  sym->metadata = RT_boxNil();

  return sym;
}

/* mem done */
Symbol *Symbol_withMeta(Symbol *self, RTValue meta) {
  if (Ptr_isReusable(self)) {
    release(self->metadata);
    self->metadata = meta;
    return self;
  }
  String *name = self->name;
  Ptr_retain(name);
  Ptr_release(self);
  return Symbol_createWithMeta(name, meta);
}

/* mem done - consumes string, consumes meta */
Symbol *Symbol_createWithMeta(String *string, RTValue meta) {
  Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
  Object_create(&sym->header, symbolType);
  sym->name = string;
  sym->ns = NULL;
  sym->metadata = meta;

  return sym;
}

String *Symbol_getName(Symbol *self) { return self->name; }

void Symbol_destroy(Symbol *self) {
  Ptr_release(self->name);
  if (self->ns)
    Ptr_release(self->ns);
  release(self->metadata);
}

bool Symbol_equals(Symbol *self, Symbol *other) {
  if (self == other)
    return true;
  return String_equals(self->name, other->name);
}

uword_t Symbol_hash(Symbol *self) { return String_hash(self->name); }

/* mem done */
String *Symbol_toString(RTValue self) {
  Symbol *sym = (Symbol *)RT_unboxSymbol(self);
  String *res = sym->name;
  Ptr_retain(res);
  Ptr_release(sym);
  return res;
}
