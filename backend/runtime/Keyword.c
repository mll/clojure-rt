#include "Object.h"
#include "Keyword.h"
#include "String.h"
#include "sds/sds.h"

Keyword* Keyword_create(String *name, String *namespace) {
  Object *super = allocate(sizeof(Keyword) + sizeof(Object)); 
  Keyword *self = (Keyword *)(super + 1);
  self->name = name;
  self->namespace = namespace;
  self->hash = combineHash(String_hash(self->name), String_hash(self->namespace));
  Object_create(super, keywordType);
  return self;
}


BOOL Keyword_equals(Keyword *self, Keyword *other) {
  return equals(super(self->name), super(other->name)) && equals(super(self->namespace), super(other->namespace));
}

uint64_t Keyword_hash(Keyword *self) {
  return self->hash; 
}

String *Keyword_toString(Keyword *self) {
  sds new = sdsnew(self->namespace->value);
  new = sdscat(new, "/");   
  new = sdscat(new, self->name->value);
  return String_create(new);
}

void Keyword_destroy(Keyword *self) {
  release(self->name);
  release(self->namespace);
}


