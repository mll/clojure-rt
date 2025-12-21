#include "Object.h"
#include "String.h"
#include <string.h>
#include "Hash.h"
#include "PersistentVector.h"

/* outside refcount system */
uint64_t String_computeHash(const char *str) {
    uint64_t h = 5381;
    int64_t c;

    while ((c = *str++)) h += avalanche_64(c);
    return h;
}

/* outside refcount system */
PersistentVector *getVec(String *s) {
  PersistentVector *vec = *((PersistentVector **) &(s->value[0]));
  assert(((Object *)vec)->type == persistentVectorType && "Wrong type");
  return vec;
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
  String *self = (String *)allocate(sizeof(String) + sizeof(char *)); 
  *((char **)&(self->value[0])) = string;
  self->count = strlen(string);
  self->hash = String_computeHash(string);
  self->specialisation = staticString;
  Object_create((Object *)self, stringType);
  return self;
}

/* mem done */
String* String_create(char *string) {
  return String_createStatic(string);
}

/* mem done */
String* String_createDynamic(size_t size) {
  String *self = (String *)allocate(sizeof(String) + sizeof(char) * (size + 1)); 
  self->count = size;
  self->specialisation = dynamicString;
  Object_create((Object *)self, stringType);
  return self;
}

String* String_createDynamicStr(const char *str) {
  size_t len = strlen(str);
  String *self = (String *)allocate(sizeof(String) + sizeof(char) * (len + 1)); 
  self->count = len;
  self->specialisation = dynamicString;
  self->hash = String_computeHash(str);
  strcpy(getDyn(self), str);
  Object_create((Object *)self, stringType);
  return self;
}


/* mem done */
String* String_createStaticOptimised(char *string, uint64_t len, uint64_t hash) {
  String *self = (String *)allocate(sizeof(String) + sizeof(char *)); 
  *((char **) &(self->value[0])) = string;
  self->count = len;
  self->hash = hash;
  self->specialisation = staticString;
  Object_create((Object *)self, stringType);
  return self;
}

/* mem done */
String* String_createCompound(String *left, String *right) {
  String *self = (String *)allocate(sizeof(String) + sizeof(PersistentVector *)); 
  self->count = left->count + right->count;
  self->specialisation = compoundString;
  self->hash = left->hash + right->hash - String_computeHash("");
  PersistentVector *v = NULL;

  if(left->specialisation != compoundString) {
    PersistentVector *empty = PersistentVector_create();
    v = PersistentVector_conj(empty, left);
  } else {
    v = getVec(left);
    assert(((Object *)getVec(left))->type == persistentVectorType && "Wrong type");
    retain(v);
    release(left);
  }
  
  if(right->specialisation != compoundString) {
    v = PersistentVector_conj(v, right);
  } else {
    assert(((Object *)getVec(right))->type == persistentVectorType && "Wrong type");
    PersistentVector *rvec = getVec(right);
    PersistentVectorIterator it = PersistentVector_iterator(rvec);
    for(uint64_t i=0; i< rvec->count; i++) { 
      retain(rvec);
      v = PersistentVector_conj(v, PersistentVector_iteratorGet(&it));
      PersistentVector_iteratorNext(&it);
    }
    release(right);
  }
  *((PersistentVector **)&(self->value[0])) = v;
  Object_create((Object *)self, stringType);
  assert(((Object *) getVec(self))->type == persistentVectorType && "Wrong type");
  return self;
}

/* outside refcount system */
char *String_iteratorGet(StringIterator *it) {
  /* no bounds check, has to always be used carefully */
  return &it->block[it->inBlockIndex];
}

/* outside refcount system */
char *String_iteratorNext(StringIterator *it) {
  if(it->inBlockIndex < it->blockLength - 1) {
    it->index++;
    it->inBlockIndex++;
    return &it->block[it->inBlockIndex];
  } 
  if(it->current->count == it->index + 1) return 0;
  
  it->inBlockIndex = 0;
  it->index++;

  String *child = (String *)PersistentVector_iteratorNext(&(it->iterator)); 
  it->blockLength = child->count;
  it->block = getStatDyn(child);
  return &it->block[it->inBlockIndex];
}

/* outside refcount system */
StringIterator String_iterator(String *self) {
  StringIterator it;
  it.index = 0;
  it.inBlockIndex = 0;
  it.current = self;
  if(self->specialisation == compoundString) {
    it.iterator = PersistentVector_iterator(getVec(self));
    String *child = (String *)PersistentVector_iteratorGet(&(it.iterator));
    it.current = child;
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
  for(uint64_t i=0; i<v->count;i++) {
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
  release(self);
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
  char leftChar = *String_iteratorGet(&left);
  char rightChar = *String_iteratorGet(&right);
  while(leftChar != 0) {
    if(leftChar != rightChar) return FALSE;
    leftChar = *String_iteratorNext(&left);
    rightChar = *String_iteratorNext(&right);
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

BOOL String_contains(String *self, String *other) {
  return String_indexOf(self, other) > -1;
}

int64_t String_indexOf(String *self, String *other) {
  return String_indexOfFrom(self, other, 0);
}

int64_t String_indexOfFrom(String *self, String *other, int64_t fromIndex) {
  int64_t selfCount = self->count, otherCount = other->count;
  
  if (fromIndex >= selfCount) {
    release(self);
    release(other);
    return (otherCount == 0 ? selfCount : -1);
  }
  if (fromIndex < 0) fromIndex = 0;
  if (otherCount == 0) {
    release(self);
    release(other);
    return fromIndex;
  }
 
  String *compactSelf = String_compactify(self);
  String *compactOther = String_compactify(other);
  // TODO: Iterator?
  char *source = self->value;
  char *target = other->value;
  
  char first = target[0];
  int64_t max = selfCount - otherCount;
 
  for (int64_t i = fromIndex; i <= max; i++) {
    if (source[i] != first) {
      while (++i <= max && source[i] != first);
    }
    if (i <= max) {
      int64_t j = i + 1, k = 1, end = j + otherCount - 1;
      while (j < end && source[j] == target[k]) {++j; ++k;}
      if (j == end) {
        /* Found whole string */
        release(compactSelf);
        release(compactOther);
        return i;
      }
    }
  }
  release(compactSelf);
  release(compactOther);
  return -1;
}

// TODO: Only the most basic version implemented: target and replacement are both one-character strings
String *String_replace(String *self, String *target, String *replacement) {
  if (String_equals(target, replacement)) {
    release(target);
    release(replacement);
    return self;
  }
  String *retVal = String_compactify(self);
  StringIterator targetIterator = String_iterator(target);
  StringIterator replacementIterator = String_iterator(replacement);
  StringIterator retValIterator = String_iterator(retVal);
  char targetChar = *String_iteratorGet(&targetIterator);
  char replacementChar = *String_iteratorGet(&replacementIterator);
  release(target);
  release(replacement);
  char *retValChar = String_iteratorGet(&retValIterator);
  while (retValChar) {
    if (*retValChar == targetChar) {
      *retValChar = replacementChar;
    }
    retValChar = String_iteratorNext(&retValIterator);
  }
  return retVal;
}
