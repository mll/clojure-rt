#include "Symbol.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <assert.h>

static void Symbol_init(Symbol *sym, String *string) {
  bool split = false;
  uword_t ns_len = 0;
  if (string->count > 1) {
    StringIterator it = String_iterator(string);
    for (uword_t i = 0; i < string->count; i++) {
      if (*String_iteratorGet(&it) == '/') {
        ns_len = i;
        split = true;
        break;
      }
      String_iteratorNext(&it);
    }
  }

  if (split) {
    Ptr_retain(string);
    sym->ns = String_subs(string, 0, ns_len);
    sym->name = String_subs(string, ns_len + 1, string->count);
  } else {
    sym->ns = NULL;
    sym->name = string;
  }

  // Assert that name never contains '/' if length > 1
  if (sym->name->count > 1) {
    StringIterator it = String_iterator(sym->name);
    for (uword_t i = 0; i < sym->name->count; i++) {
      assert(*String_iteratorGet(&it) != '/' && "Symbol name (length > 1) cannot contain '/'");
      String_iteratorNext(&it);
    }
  }
}

/* mem done - consumes string */
Symbol *Symbol_create(String *string) {
  Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
  Object_create(&sym->header, symbolType);
  Symbol_init(sym, string);
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
  String *ns = self->ns;
  if (ns) {
    Ptr_retain(ns);
  }
  Ptr_release(self);

  Symbol *sym = Symbol_createWithMeta(name, meta);
  if (ns) {
    sym->ns = ns;
  }
  return sym;
}

/* mem done - consumes string, consumes meta */
Symbol *Symbol_createWithMeta(String *string, RTValue meta) {
  Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
  Object_create(&sym->header, symbolType);
  Symbol_init(sym, string);
  sym->metadata = meta;

  return sym;
}

String *Symbol_getName(Symbol *self) {
  String *name = self->name;
  Ptr_retain(name);
  Ptr_release(self);
  return name;
}

RTValue Symbol_getMeta(Symbol *self) {
  RTValue meta = self->metadata;
  retain(meta);
  Ptr_release(self);
  return meta;
}

void Symbol_destroy(Symbol *self) {
  Ptr_release(self->name);
  if (self->ns)
    Ptr_release(self->ns);
  release(self->metadata);
}

bool Symbol_equals(Symbol *self, Symbol *other) {
  if (self == other)
    return true;

  if (self->ns == NULL && other->ns != NULL)
    return false;
  if (self->ns != NULL && other->ns == NULL)
    return false;
  if (self->ns != NULL && !String_equals(self->ns, other->ns))
    return false;

  return String_equals(self->name, other->name);
}

uword_t Symbol_hash(Symbol *self) {
  uword_t h = String_hash(self->name);
  if (self->ns) {
    h ^= String_hash(self->ns);
  }
  return h;
}

/* mem done */
String *Symbol_toString(RTValue self) {
  Symbol *sym = (Symbol *)RT_unboxSymbol(self);
  String *res = NULL;
  if (sym->ns != NULL) {
    String *ns = sym->ns;
    Ptr_retain(ns);
    String *slash = String_create("/");
    String *name = sym->name;
    Ptr_retain(name);
    
    res = String_concat(ns, slash);
    res = String_concat(res, name);
  } else {
    res = sym->name;
    Ptr_retain(res);
  }
  Ptr_release(sym);
  return res;
}
