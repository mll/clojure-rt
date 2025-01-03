#include "Object.h"
#include "PersistentArrayMap.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"

static PersistentArrayMap *EMPTY = NULL;

/* mem done */
PersistentArrayMap* PersistentArrayMap_empty() {
  if (EMPTY == NULL) EMPTY = PersistentArrayMap_create();
  retain(EMPTY);
  return EMPTY;
} 

/* mem done */
PersistentArrayMap* PersistentArrayMap_create() {
  PersistentArrayMap *self = allocate(sizeof(PersistentArrayMap)); 
  self->count = 0;
  Object_create((Object *)self, persistentArrayMapType);
  return self;
}

/* outside refcount system */
PersistentArrayMap* PersistentArrayMap_copy(PersistentArrayMap *other) {
  size_t size = sizeof(PersistentArrayMap);
  PersistentArrayMap *self = allocate(size); 
  Object_create((Object *)self, persistentArrayMapType);
  self->count = other->count;
  memcpy(self->keys, other->keys, sizeof(void *) * self->count);
  memcpy(self->values, other->values, sizeof(void *) * self->count);

  return self;
}

/* mem done */
PersistentArrayMap* PersistentArrayMap_createMany(uint64_t pairCount, ...) {
  assert(pairCount < HASHTABLE_THRESHOLD+1 && "Maps of size > 8 not supported yet");
  va_list args;
  va_start(args, pairCount);

  PersistentArrayMap *self = PersistentArrayMap_create();
  
  self->count = pairCount;
  for(int i=0; i<pairCount; i++) {
    void *k = va_arg(args, void *);
    void *v = va_arg(args, void *);
    self->keys[i] = k;
    self->values[i] = v;
  }
  va_end(args);
  return self;
}

/* outside refcount system */
BOOL PersistentArrayMap_equals(PersistentArrayMap *self, PersistentArrayMap *other) {
  if(self->count != other->count) return FALSE;
  for(int i=0; i< self->count; i++) {
    void *key = self->keys[i];
    void *val = self->values[i];
    retain(self);
    retain(key);
    void *otherVal = PersistentArrayMap_get(other, key);
    if(!equals(otherVal, val)) return FALSE;
  }
  return TRUE;
}

/* outside refcount system */
uint64_t PersistentArrayMap_hash(PersistentArrayMap *self) {
    uint64_t h = 5381; // hashing should be cached?
    for(int i=0; i<self->count; i++) {
      void *key = self->keys[i];
      void *value = self->values[i];
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

  BOOL hasAtLeastOne = FALSE;
  for(int i=0; i<self->count; i++) {
      void *key = self->keys[i];
      if(hasAtLeastOne) {
        retain(comma);
        retVal = String_concat(retVal, comma);
      }
      hasAtLeastOne = TRUE;
      retain(key);
      String *ks = toString(key);
      retVal = String_concat(retVal, ks);
      retain(space);
      retVal = String_concat(retVal, space);
      retain(self->values[i]);
      String *vs = toString(self->values[i]);
      retVal = String_concat(retVal, vs);
  }
  
  retVal = String_concat(retVal, closing); 
  release(space);
  release(comma);
  release(self);
  return retVal;
}

/* outside refcount system */
void PersistentArrayMap_destroy(PersistentArrayMap *self, BOOL deallocateChildren) {
  if(deallocateChildren) {
    for(int i=0; i<self->count; i++) {
      void *key = self->keys[i];
      void *value = self->values[i];
      release(key); 
      release(value);  
    }
  }
}

/* outside refcount system */
void PersistentArrayMap_retainChildren(PersistentArrayMap *self, int except) {
 for(int i=0; i<self->count; i++) {
    void *foundKey = self->keys[i];
    if(foundKey && except != i) {
      retain(self->keys[i]);
      retain(self->values[i]);
    }
 }
}

/* mem done */
PersistentArrayMap* PersistentArrayMap_assoc(PersistentArrayMap *self, void *key, void *value) {
  retain(self);
  retain(key);
  int64_t found = PersistentArrayMap_indexOf(self, key);
  BOOL reusable = isReusable(self);
  if(found == -1) {
    assert(self->count < HASHTABLE_THRESHOLD && "Maps of size > 8 not supported yet");
    PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
    self->keys[retVal->count] = key;
    self->values[retVal->count] = value;
    retVal->count++;
    if(!reusable) {
      PersistentArrayMap_retainChildren(retVal, retVal->count-1);
      release(self);
    }
    return retVal;
  }
  
  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  if(reusable) release(retVal->values[found]);
  retVal->values[found] = value;
  if(!reusable) { 
    PersistentArrayMap_retainChildren(retVal, found); 
    release(self);
  }
  release(key);
  return retVal;
}

/* mem done */
PersistentArrayMap* PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key) {
  retain(self);
  int64_t found = PersistentArrayMap_indexOf(self, key);
  if(found == -1) {
    return self;
  }
  BOOL reusable = isReusable(self);
  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  void *k = retVal->keys[found];
  void *v = retVal->values[found];
  for(int64_t i=found; i<self->count-1;i++) {
    retVal->keys[i] = self->keys[i+1];
    retVal->values[i] = self->values[i+1];
  }
  retVal->count--;
  if(!reusable) {
    PersistentArrayMap_retainChildren(retVal, -1);
    release(self);
  }
  else {
    release(k);
    release(v);
  }
  return retVal;
}

/* mem done */
int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key) {
  int64_t retVal = -1;
  for(int64_t i=0; i<self->count; i++) {
    if(equals(key, self->keys[i])) {
      retVal = i;
      break;
    }
  }
  release(self);
  release(key);  
  return retVal;
}

/* mem done */
void* PersistentArrayMap_get(PersistentArrayMap *self, void *key) {
  void *retVal = NULL;
  for(int64_t i=0; i<self->count; i++) {
    if(equals(key, self->keys[i])) {
      retVal = self->values[i];
      break;
    }
  }
  if(retVal) retain(retVal);
  release(self);
  release(key);  
  return retVal ?: Nil_create();
}

/* mem done */
void* PersistentArrayMap_dynamic_get(void *self, void *key) {
  Object *type = (Object *) self;
  if(type->type != persistentArrayMapType) { 
    release(self);
    release(key);    
    return Nil_create();
  }
  return PersistentArrayMap_get(self, key);
}

