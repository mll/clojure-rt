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
  retain(string);
  void *retVal = ConcurrentHashMap_get(keywords, string);

  if(retVal == UNIQUE_NIL) { /* interning */
    release(retVal);
    retain(string);
    Keyword *new = Keyword_allocate(string);
    retain(new);
    /* TODO - what if another thread has interned first? */
    ConcurrentHashMap_assoc(keywords, string, new);
    return new;
  }
  release(string);
  return retVal;
}

/* outside refcount system */
BOOL Keyword_equals(Keyword *self, Keyword *other) {
  /* What if another thread interns? */
  /* what is here does not matter - keyword equality is checked based on a pointer only */
  return FALSE; //equals(self->string, other->string);
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
  /* In practice this will never happen for keywords allocated within the compiler, because each time a keyword is used, it is retained. */
  release(self->string);
}





