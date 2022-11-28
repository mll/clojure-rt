#include "Object.h"
#include "String.h"
#include <string.h>
#include "Hash.h"
#include "PersistentVector.h"

/* outside refcount system */
uint64_t String_computeHash(char *str) {
    uint64_t h = 5381;
    int64_t c;

    while ((c = *str++)) h += avalanche_64(c);
    return h;
}

/* outside refcount system */
PersistentVector *getVec(String *s) {
  return *((PersistentVector **) &(s->value[0]));
}

/* outside refcount system */
char *getStat(String *s) {
  return *((char **) &(s->value[0]));
}

/* outside refcount system */
char *getDyn(String *s) {
  return &(s->value[0]);
}

/* outside refcount system */
char* getStatDyn(String *s) {
  return s->specialisation == staticString ? getStat(s) : getDyn(s);
}

/* outside refcount system */
void String_recomputeHash(String *s) {
  assert(s->specialisation != compoundString);
  s->hash = String_computeHash(getStatDyn(s));
}

/* mem done */
String* String_createStatic(char *string) {
  Object *super = allocate(sizeof(String) + sizeof(Object) + sizeof(char *)); 
  String *self = (String *)(super + 1);
  *((char **)&(self->value[0])) = string;
  self->count = strlen(string);
  self->hash = String_computeHash(string);
  self->specialisation = staticString;
  Object_create(super, stringType);
  return self;
}

/* mem done */
String* String_create(char *string) {
  return String_createStatic(string);
}

/* mem done */
String* String_createDynamic(size_t size) {
  Object *super = allocate(sizeof(String) + sizeof(Object) + sizeof(char) * (size + 1)); 
  String *self = (String *)(super + 1);
  self->count = size;
  self->specialisation = dynamicString;
  Object_create(super, stringType);
  return self;
}

/* mem done */
String* String_createStaticOptimised(char *string, uint64_t len, uint64_t hash) {
  Object *super = allocate(sizeof(String) + sizeof(Object) + sizeof(char *)); 
  String *self = (String *)(super + 1);
  *((char **) &(self->value[0])) = string;
  self->count = len;
  self->hash = hash;
  self->specialisation = staticString;
  Object_create(super, stringType);
  return self;
}

String* String_createCompound(String *left, String *right) {
  Object *sup = allocate(sizeof(String) + sizeof(Object) + sizeof(PersistentVector *)); 
  String *self = (String *)(sup + 1);
  self->count = left->count + right->count;
  self->specialisation = compoundString;
  self->hash = left->hash + right->hash - String_computeHash("");
  PersistentVector *v = NULL;
  
  if(left->specialisation != compoundString) {
    PersistentVector *empty = PersistentVector_create();
    v = PersistentVector_conj(empty, left);
  } else {
    v = getVec(left);
    retain(v);
    release(left);
  }
  
  if(right->specialisation != compoundString) {
    PersistentVector *added = PersistentVector_conj(v, right);
    v = added;
  } else {
    PersistentVector *rvec = getVec(right);
    release(right);
    for(int i=0; i< rvec->count; i++) { /* TODO - use transients and iterators here */
      PersistentVector *added = PersistentVector_conj(v, PersistentVector_nth(rvec, i));
      v = added;
    }
  }
  *((PersistentVector **)&(self->value[0])) = v;
  Object_create(sup, stringType);
  return self;
}

/* outside refcount system */
char String_iteratorGetChar(String *self, StringIterator *it) {
  return it->block[it->inBlockIndex];
}

/* outside refcount system */
char String_iteratorNext(String *self, StringIterator *it) {
  if(self->count == it->index) return 0;
  if(it->inBlockIndex < it->blockLength - 1) {
    it->index++;
    it->inBlockIndex++;
    return it->block[it->inBlockIndex];
  } 
  it->inBlockIndex = 0;
  it->index++;
  it->blockIndex++;
  /* TODO - use vector iterator */
  String *child = PersistentVector_nth(getVec(self), it->blockIndex);
  it->blockLength = child->count;
  it->block = getStatDyn(child);
  return it->block[it->inBlockIndex];
}

/* outside refcount system */
StringIterator String_iterator(String *self) {
  StringIterator it;
  it.index = 0;
  it.inBlockIndex = 0;
  it.blockIndex = 0;
  if(self->specialisation == compoundString) {
    String *child = PersistentVector_nth(getVec(self), 0);
    it.blockLength = child->count;
    it.block = getStatDyn(child);
    return it;
  }
  it.block = getStatDyn(self);
  it.blockLength = self->count;
  return it;
}

/* mem done */
String *String_compactify(String *self) {
  if(self->specialisation != compoundString) { 
    return self;
  }

  String *out = String_createDynamic(self->count);
  char *output = &(out->value[0]);
  
  PersistentVector *v = getVec(self);

  int start = 0;
  for(int i=0; i<v->count;i++) {
    /* TODO - use vector iterator */
    retain(v);
    String *block = PersistentVector_nth(v, i);
    char *blockPtr = getStatDyn(block);
    memcpy(output + start, blockPtr, block->count);
    start += block->count;
    release(block);
  }
  output[self->count] = 0;
  out->hash = String_computeHash(output);
  return out;
}

/* outside refcount system */
char *String_c_str(String *self) {
  assert(self->specialisation != compoundString);
  return getStatDyn(self);
}

/* outside refcount system */
BOOL String_equals(String *self, String *other) {
  if(self->count != other->count) return FALSE;
  if(self->specialisation != compoundString && other->specialisation!= compoundString) {
    char *left = getStatDyn(self);
    char *right = getStatDyn(other);
    return strcmp(left, right) == 0;    
  }
 
  StringIterator left = String_iterator(self);
  StringIterator right = String_iterator(other);
  char leftChar = String_iteratorGetChar(self, &left);
  char rightChar = String_iteratorGetChar(other, &right);
  while(leftChar != 0) {
    if(leftChar != rightChar) return FALSE;
    leftChar = String_iteratorNext(self, &left);
    rightChar = String_iteratorNext(other, &right);
  }
  return TRUE;
}

/* outside refcount system */
uint64_t String_hash(String *self) {
  return self->hash;
}

/* mem done */
String *String_toString(String *self) {
  return self;
}

/* outside refcount system */
void String_destroy(String *self) {
  if(self->specialisation == compoundString) release(getVec(self));
}

/* mem done */
String *String_concat(String *self, String *other) {
  return String_createCompound(self, other);
}

