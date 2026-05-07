#include "String.h"
#include "Exceptions.h"
#include "Hash.h"
#include "Object.h"
#include "PersistentVector.h"
#include "RTValue.h"
#include <string.h>

/* outside refcount system */
uword_t String_computeHash(const char *str) {
  uword_t h = 5381;
  word_t c;

  while ((c = *str++))
    h += avalanche(c);
  return h;
}

/* outside refcount system */
PersistentVector *getVec(String *s) {
  PersistentVector *vec = *((PersistentVector **)&(s->value[0]));
  assert(((Object *)vec)->type == persistentVectorType && "Wrong type");
  return vec;
}

/* outside refcount system */
char *getStat(String *s) { return *((char **)&(s->value[0])); }

/* outside refcount system */
char *getDyn(String *s) { return &(s->value[0]); }

/* outside refcount system */
char *getStatDyn(String *s) {
  return s->specialisation == staticString ? getStat(s) : getDyn(s);
}

/* outside refcount system */
void String_recomputeHash(String *s) {
  assert(s->specialisation != compoundString);
  s->hash = String_computeHash(getStatDyn(s));
}

/* mem done */
String *String_createStatic(const char *string) {
  String *self = (String *)allocate(sizeof(String) + sizeof(char *));
  *((const char **)&(self->value[0])) = string;
  self->count = strlen(string);
  self->hash = String_computeHash(string);
  self->specialisation = staticString;
  Object_create((Object *)self, stringType);
  return self;
}

/* mem done */
String *String_create(const char *string) {
  return String_createStatic(string);
}

String *String_createEmpty() { return String_createStatic(""); }

/* mem done */
String *String_createDynamic(size_t size) {
  String *self = (String *)allocate(sizeof(String) + sizeof(char) * (size + 1));
  self->count = size;
  self->specialisation = dynamicString;
  Object_create((Object *)self, stringType);
  return self;
}

String *String_createDynamicStr(const char *str) {
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
String *String_createStaticOptimised(char *string, uword_t len, uword_t hash) {
  String *self = (String *)allocate(sizeof(String) + sizeof(char *));
  *((char **)&(self->value[0])) = string;
  self->count = len;
  self->hash = hash;
  self->specialisation = staticString;
  Object_create((Object *)self, stringType);
  return self;
}

/* mem done */
String *String_createCompound(String *left, String *right) {
  String *self =
      (String *)allocate(sizeof(String) + sizeof(PersistentVector *));
  self->count = left->count + right->count;
  self->specialisation = compoundString;
  self->hash = left->hash + right->hash - String_computeHash("");
  assert(self->hash != 0);
  PersistentVector *v = NULL;

  if (left->specialisation != compoundString) {
    PersistentVector *empty = PersistentVector_create();
    v = PersistentVector_conj(empty, RT_boxPtr(left));
  } else {
    v = getVec(left);
    assert(((Object *)getVec(left))->type == persistentVectorType &&
           "Wrong type");
    Ptr_retain(v);
    Ptr_release(left);
  }

  if (right->specialisation != compoundString) {
    v = PersistentVector_conj(v, RT_boxPtr(right));
  } else {
    assert(((Object *)getVec(right))->type == persistentVectorType &&
           "Wrong type");
    PersistentVector *rvec = getVec(right);
    PersistentVectorIterator it = PersistentVector_iterator(rvec);
    for (uword_t i = 0; i < rvec->count; i++) {
      retain(PersistentVector_iteratorGet(&it));
      v = PersistentVector_conj(v, PersistentVector_iteratorGet(&it));
      PersistentVector_iteratorNext(&it);
    }
    Ptr_release(right);
  }
  *((PersistentVector **)&(self->value[0])) = v;
  Object_create((Object *)self, stringType);
  assert(((Object *)getVec(self))->type == persistentVectorType &&
         "Wrong type");
  return self;
}

/* outside refcount system */
char *String_iteratorGet(StringIterator *it) {
  /* no bounds check, has to always be used carefully */
  return &it->block[it->inBlockIndex];
}

/* outside refcount system */
char *String_iteratorNext(StringIterator *it) {
  if (it->inBlockIndex < it->blockLength - 1) {
    it->index++;
    it->inBlockIndex++;
    return &it->block[it->inBlockIndex];
  }
  if (it->current->count == it->index + 1)
    return 0;

  it->inBlockIndex = 0;
  it->index++;

  String *child =
      (String *)RT_unboxPtr(PersistentVector_iteratorNext(&(it->iterator)));
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
  if (self->specialisation == compoundString) {
    it.iterator = PersistentVector_iterator(getVec(self));
    String *child =
        (String *)RT_unboxPtr(PersistentVector_iteratorGet(&(it.iterator)));
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
  if (self->specialisation != compoundString) {
    return self;
  }

  String *out = String_createDynamic(self->count);
  char *output = &(out->value[0]);

  PersistentVector *v = getVec(self);

  int start = 0;
  PersistentVectorIterator it = PersistentVector_iterator(v);
  for (uword_t i = 0; i < v->count; i++) {
    RTValue val = PersistentVector_iteratorGet(&it);
    // Iterator get DOES NOT consume val, but we need to unbox it.
    String *block = (String *)RT_unboxPtr(val);
    assert(block->specialisation != compoundString);
    char *blockPtr = getStatDyn(block);
    memcpy(output + start, blockPtr, block->count);
    start += block->count;
    PersistentVector_iteratorNext(&it);
  }
  output[self->count] = 0;
  out->hash = String_computeHash(output);

  Ptr_release(self);
  return out;
}

/* outside refcount system */
const char *String_c_str(String *self) {
  assert(self->specialisation != compoundString);
  return (const char *)getStatDyn(self);
}

/* outside refcount system */
bool String_equals(String *self, String *other) {
  if (self->count != other->count)
    return false;
  if (self->specialisation != compoundString &&
      other->specialisation != compoundString) {
    char *left = getStatDyn(self);
    char *right = getStatDyn(other);
    return strcmp(left, right) == 0;
  }

  StringIterator left = String_iterator(self);
  StringIterator right = String_iterator(other);
  char leftChar = *String_iteratorGet(&left);
  char rightChar = *String_iteratorGet(&right);
  while (leftChar != 0) {
    if (leftChar != rightChar)
      return false;
    leftChar = *String_iteratorNext(&left);
    rightChar = *String_iteratorNext(&right);
  }
  return true;
}

/* outside refcount system */
uword_t String_hash(String *self) { return self->hash; }

/* mem done */
String *String_toString(String *self) { return self; }

/* outside refcount system */
void String_destroy(String *self) {
  if (self->specialisation == compoundString)
    Ptr_release(getVec(self));
}

/* mem done */
String *String_concat(String *self, String *other) {
  return String_createCompound(self, other);
}

bool String_contains(String *self, String *other) {
  return String_indexOf(self, other) > -1;
}

word_t String_indexOf(String *self, String *other) {
  return String_indexOfFrom(self, other, 0);
}

/* mem done */
/* Checks if `self` contains `other` starting from `fromIndex`.
 * Returns index of first character of `other` in `self` or -1 if not found.
 */
word_t String_indexOfFrom(String *self, String *other, word_t fromIndex) {
  word_t selfCount = self->count, otherCount = other->count;

  if (fromIndex >= selfCount) {
    Ptr_release(self);
    Ptr_release(other);
    return (otherCount == 0 ? selfCount : -1);
  }
  if (fromIndex < 0)
    fromIndex = 0;
  if (otherCount == 0) {
    Ptr_release(self);
    Ptr_release(other);
    return fromIndex;
  }

  String *compactSelf = String_compactify(self);
  String *compactOther = String_compactify(other);
  // TODO: Iterator?
  const char *source = String_c_str(compactSelf);
  const char *target = String_c_str(compactOther);

  char first = target[0];
  word_t max = selfCount - otherCount;

  for (word_t i = fromIndex; i <= max; i++) {
    if (source[i] != first) {
      while (++i <= max && source[i] != first)
        ;
    }
    if (i <= max) {
      word_t j = i + 1, k = 1, end = j + otherCount - 1;
      while (j < end && source[j] == target[k]) {
        ++j;
        ++k;
      }
      if (j == end) {
        /* Found whole string */
        Ptr_release(compactSelf);
        Ptr_release(compactOther);
        return i;
      }
    }
  }
  Ptr_release(compactSelf);
  Ptr_release(compactOther);
  return -1;
}

String *String_replace(String *self, String *target, String *replacement) {
  if (target == replacement || String_equals(target, replacement)) {
    Ptr_release(target);
    Ptr_release(replacement);
    return self;
  }
  // TODO: Only the most basic version implemented: target and replacement are
  // both one-character strings

  if (target->count != 1 || replacement->count != 1) {
    Ptr_release(self);
    Ptr_release(target);
    Ptr_release(replacement);
    throwUnsupportedOperationException_C(
        "replace is currently only supported for single-character strings");
  }

  String *compactified = String_compactify(self);
  String *retVal = String_createDynamic(compactified->count);
  memcpy(getDyn(retVal), String_c_str(compactified), compactified->count);
  getDyn(retVal)[compactified->count] = '\0';
  Ptr_release(compactified);

  StringIterator targetIterator = String_iterator(target);
  StringIterator replacementIterator = String_iterator(replacement);
  StringIterator retValIterator = String_iterator(retVal);
  char targetChar = *String_iteratorGet(&targetIterator);
  char replacementChar = *String_iteratorGet(&replacementIterator);
  Ptr_release(target);
  Ptr_release(replacement);
  char *retValChar = String_iteratorGet(&retValIterator);
  while (retValChar) {
    if (*retValChar == targetChar) {
      *retValChar = replacementChar;
    }
    retValChar = String_iteratorNext(&retValIterator);
  }
  return retVal;
}

char String_charAt(String *self, uword_t index) {
  if (index >= self->count) {
    uword_t cnt = self->count;
    Ptr_release(self);
    throwIndexOutOfBoundsException_C(index, cnt);
  }

  StringIterator it = String_iterator(self);
  for (uword_t i = 0; i < index; i++) {
    String_iteratorNext(&it);
  }
  char result = *String_iteratorGet(&it);
  Ptr_release(self);
  return result;
}

String *String_subs(String *self, uword_t start, uword_t end) {
  if (start > end || end > self->count) {
    uword_t cnt = self->count;
    Ptr_release(self);
    throwIndexOutOfBoundsException_C(end, cnt);
  }

  uword_t len = end - start;
  String *res = String_createDynamic(len);
  char *dest = getDyn(res);

  StringIterator it = String_iterator(self);
  for (uword_t i = 0; i < start; i++) {
    String_iteratorNext(&it);
  }

  for (uword_t i = 0; i < len; i++) {
    dest[i] = *String_iteratorGet(&it);
    String_iteratorNext(&it);
  }
  dest[len] = '\0';
  res->hash = String_computeHash(dest);

  Ptr_release(self);
  return res;
}
