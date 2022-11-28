#include "Object.h"
#include "Keyword.h"
#include "String.h"
#include "ConcurrentHashMap.h"

extern ConcurrentHashMap *keywords;
extern Nil *UNIQUE_NIL;

/* mem done */
Keyword* Keyword_allocate(String *string) {
  Object *super = allocate(sizeof(Keyword) + sizeof(Object)); 
  Keyword *self = (Keyword *)(super + 1);
  self->string = string;
  Object_create(super, keywordType);
  return self;
}

/* mem done */
Keyword* Keyword_create(String *string) {
  /* TODO ConcurrentHashMap mem */
  retain(keywords);
  retain(string);
  Object *retVal = ConcurrentHashMap_get(keywords, super(string));

  if(Object_data(retVal) == UNIQUE_NIL) { /* interning */
    Object_release(retVal);
    retain(string);
    Keyword *new = Keyword_allocate(string);
    retain(keywords);
    ConcurrentHashMap_assoc(keywords, super(string), super(new));
    /* TODO mem */
    release(new); /* Hash map should not be an owner of the keyword, so we reverse its retain */
    return new;
  }
  return Object_data(retVal);
}

/* outside refcount system */
BOOL Keyword_equals(Keyword *self, Keyword *other) {
  /* because we are uniquing / interning */
  return self == other;
}

/* outside refcount system */
uint64_t Keyword_hash(Keyword *self) {
    return self->string->hash;
}

/* mem done */
String *Keyword_toString(Keyword *self) {
  String *colon = String_create(":");
  retain(self->string);
  String *result = String_concat(colon, self->string);
  release(self);
  return result;
}

/* outside refcount system */
void Keyword_destroy(Keyword *self) {
  ConcurrentHashMap_dissoc(keywords, super(self->string));
  release(self->string);
}





