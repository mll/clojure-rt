#include "Object.h"
#include "Keyword.h"
#include "String.h"
#include "ConcurrentHashMap.h"

extern ConcurrentHashMap *keywords;
extern Nil *UNIQUE_NIL;

Keyword* Keyword_allocate(String *string) {
  Object *super = allocate(sizeof(Keyword) + sizeof(Object)); 
  Keyword *self = (Keyword *)(super + 1);
  self->string = string;
  retain(string);
  Object_create(super, keywordType);
  return self;
}

Keyword* Keyword_create(String *string) {
  Object *retVal = ConcurrentHashMap_get(keywords, super(string));
  if(Object_data(retVal) == UNIQUE_NIL) { /* interning */
    Object_release(retVal);
    Keyword *new = Keyword_allocate(string);
    ConcurrentHashMap_assoc(keywords, super(string), super(new));
    release(new); /* Hash map should not be an owner of the keyword, so we reverse its retain */
    return new;
  }
  Object_retain(retVal);
  return Object_data(retVal);
}

BOOL Keyword_equals(Keyword *self, Keyword *other) {
  /* because we are uniquing / interning */
  return self == other;
}

uint64_t Keyword_hash(Keyword *self) {
    return self->string->hash;
}

String *Keyword_toString(Keyword *self) {
  retain(self->string);
  return self->string;
}

void Keyword_destroy(Keyword *self) {
  ConcurrentHashMap_dissoc(keywords, super(self->string));
  release(self->string);
}





