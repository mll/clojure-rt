#include "PersistentArrayMap.h"
#include "Nil.h"
#include "Object.h"
#include "RTValue.h"
#include "defines.h"
#include <stdarg.h>

static PersistentArrayMap *EMPTY = NULL;

/* mem done */
PersistentArrayMap *PersistentArrayMap_empty() {
  if (EMPTY == NULL)
    EMPTY = PersistentArrayMap_create();
  Ptr_retain(EMPTY);
  return EMPTY;
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_create() {
  PersistentArrayMap *self = allocate(sizeof(PersistentArrayMap));
  self->count = 0;
  Object_create((Object *)self, persistentArrayMapType);
  return self;
}

/* outside refcount system */
PersistentArrayMap *PersistentArrayMap_copy(PersistentArrayMap *other) {
  size_t size = sizeof(PersistentArrayMap);
  PersistentArrayMap *self = allocate(size);
  Object_create((Object *)self, persistentArrayMapType);
  self->count = other->count;
  memcpy(self->keys, other->keys, sizeof(RTValue) * self->count);
  memcpy(self->values, other->values, sizeof(RTValue) * self->count);

  return self;
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_createMany(int32_t pairCount, ...) {
  assert(pairCount < HASHTABLE_THRESHOLD + 1 &&
         "Maps of size > 8 not supported yet");
  va_list args;
  va_start(args, pairCount);

  PersistentArrayMap *self = PersistentArrayMap_create();

  self->count = pairCount;
  for (uword_t i = 0; i < pairCount; i++) {
    RTValue k = va_arg(args, RTValue);
    RTValue v = va_arg(args, RTValue);
    self->keys[i] = k;
    self->values[i] = v;
  }
  va_end(args);
  return self;
}

/* outside refcount system */
bool PersistentArrayMap_equals(PersistentArrayMap *self,
                               PersistentArrayMap *other) {
  if (self->count != other->count)
    return false;
  for (uword_t i = 0; i < self->count; i++) {
    RTValue key = self->keys[i];
    RTValue val = self->values[i];
    Ptr_retain(other);
    retain(key);
    RTValue otherVal = PersistentArrayMap_get(other, key);
    if (!equals(otherVal, val)) {
      release(otherVal);
      return false;
    }
  }
  return true;
}

/* outside refcount system */
uword_t PersistentArrayMap_hash(PersistentArrayMap *self) {
  uword_t h = 5381; // hashing should be cached?
  for (uword_t i = 0; i < self->count; i++) {
    RTValue key = self->keys[i];
    RTValue value = self->values[i];
    h += (hash(key) ^ hash(value));
  }
  return h;
}

/* mem done */
String *PersistentArrayMap_toString(PersistentArrayMap *self) {
  String *retVal = String_create("{");
  String *space = String_create(" ");
  String *comma = String_create(", ");
  String *closing = String_create("}");

  bool hasAtLeastOne = false;
  for (uword_t i = 0; i < self->count; i++) {
    RTValue key = self->keys[i];
    if (hasAtLeastOne) {
      Ptr_retain(comma);
      retVal = String_concat(retVal, comma);
    }
    hasAtLeastOne = true;
    retain(key);
    String *ks = toString(key);
    retVal = String_concat(retVal, ks);
    Ptr_retain(space);
    retVal = String_concat(retVal, space);
    retain(self->values[i]);
    String *vs = toString(self->values[i]);
    retVal = String_concat(retVal, vs);
  }

  retVal = String_concat(retVal, closing);
  Ptr_release(space);
  Ptr_release(comma);
  Ptr_release(self);
  return retVal;
}

/* outside refcount system */
void PersistentArrayMap_destroy(PersistentArrayMap *self,
                                bool deallocateChildren) {
  if (deallocateChildren) {
    for (uword_t i = 0; i < self->count; i++) {
      RTValue key = self->keys[i];
      RTValue value = self->values[i];
      release(key);
      release(value);
    }
  }
}

/* outside refcount system */
void PersistentArrayMap_retainChildren(PersistentArrayMap *self, int except) {
  for (uword_t i = 0; i < self->count; i++) {
    RTValue foundKey = self->keys[i];
    if (foundKey && except != (int)i) {
      retain(self->keys[i]);
      retain(self->values[i]);
    }
  }
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_assoc(PersistentArrayMap *self,
                                             RTValue key, RTValue value) {
  Ptr_retain(self);
  retain(key);
  word_t found = PersistentArrayMap_indexOf(self, key);
  bool reusable = Ptr_isReusable(self);
  if (found == -1) {
    assert(self->count < HASHTABLE_THRESHOLD &&
           "Maps of size > 8 not supported yet");
    PersistentArrayMap *retVal =
        reusable ? self : PersistentArrayMap_copy(self);
    retVal->keys[retVal->count] = key;
    retVal->values[retVal->count] = value;
    retVal->count++;
    if (!reusable) {
      PersistentArrayMap_retainChildren(retVal, retVal->count - 1);
      Ptr_release(self);
    }
    return retVal;
  }

  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  if (reusable)
    release(retVal->values[found]);
  retVal->values[found] = value;
  if (!reusable) {
    PersistentArrayMap_retainChildren(retVal, found);
    Ptr_release(self);
  }
  release(key);
  return retVal;
}

/* mem done */
PersistentArrayMap *PersistentArrayMap_dissoc(PersistentArrayMap *self,
                                              RTValue key) {
  Ptr_retain(self);
  word_t found = PersistentArrayMap_indexOf(self, key);
  if (found == -1) {
    return self;
  }
  bool reusable = Ptr_isReusable(self);
  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  RTValue k = retVal->keys[found];
  RTValue v = retVal->values[found];
  for (uword_t i = found; i < self->count - 1; i++) {
    retVal->keys[i] = self->keys[i + 1];
    retVal->values[i] = self->values[i + 1];
  }
  retVal->count--;
  if (!reusable) {
    PersistentArrayMap_retainChildren(retVal, -1);
    Ptr_release(self);
  } else {
    release(k);
    release(v);
  }
  return retVal;
}

/* mem done */
word_t PersistentArrayMap_indexOf(PersistentArrayMap *self, RTValue key) {
  word_t retVal = -1;
  for (uword_t i = 0; i < self->count; i++) {
    if (equals(key, self->keys[i])) {
      retVal = i;
      break;
    }
  }
  Ptr_release(self);
  release(key);
  return retVal;
}

/* mem done */
RTValue PersistentArrayMap_get(PersistentArrayMap *self, RTValue key) {
  RTValue retVal = RT_boxNull();
  for (uword_t i = 0; i < self->count; i++) {
    if (equals(key, self->keys[i])) {
      retVal = self->values[i];
      if (!RT_isNull(retVal))
        retain(retVal);
      break;
    }
  }
  Ptr_release(self);
  release(key);

  if (!RT_isNull(retVal)) {
    return retVal;
  }

  return RT_boxNil();
}

/* mem done */
RTValue PersistentArrayMap_dynamic_get(RTValue self, RTValue key) {

  if (getType(self) != persistentArrayMapType) {
    release(self);
    release(key);
    return RT_boxNil();
  }
  return PersistentArrayMap_get(RT_unboxPtr(self), key);
}
