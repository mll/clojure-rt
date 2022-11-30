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
    /* TODO - what if another thread has interned first? */
    ConcurrentHashMap_assoc(keywords, string, new);
    retain(new);
//    printf("Keyword: %llu %llu\n", (uint64_t)new, super(new)->atomicRefCount);
    fflush(stdout);
    return new;
  }
  release(string);
//  printf("Keyword: %llu %llu\n", (uint64_t)retVal, super(retVal)->atomicRefCount);
  fflush(stdout);
  return retVal;
}

/* outside refcount system */
BOOL Keyword_equals(Keyword *self, Keyword *other) {
  /* What if another thread interns? */
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
//  printf("DESTROY!!!!!!! %llu\n", (uint64_t) self);
  fflush(stdout);
  release(self->string);
}





