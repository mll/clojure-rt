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
  size_t size = sizeof(PersistentArrayMap) + sizeof(Object);
  Object *super = allocate(size); 
  PersistentArrayMap *self = Object_data(super);
  memset(super, 0, size);
  Object_create(super, persistentArrayMapType);
  return self;
}

/* outside refcount system */
PersistentArrayMap* PersistentArrayMap_copy(PersistentArrayMap *other) {
  size_t baseSize = sizeof(PersistentArrayMap);
  size_t size = baseSize + sizeof(Object);
  Object *super = allocate(size); 
  PersistentArrayMap *self = Object_data(super);
  memcpy(self, other, baseSize);
  Object_create(super, persistentArrayMapType);
  return self;
}

/* mem done */
PersistentArrayMap* PersistentArrayMap_createMany(uint64_t pairCount, ...) {
  va_list args;
  va_start(args, pairCount);

  PersistentArrayMap *self = PersistentArrayMap_create();

  self->count = pairCount;
  for(int i=0; i<pairCount; i++) {
    void *k = va_arg(args, void *);
    void *v = va_arg(args, void *);
    self = PersistentArrayMap_assoc(self, k, v);
  }
  va_end(args);
  return self;
}

/* outside refcount system */
BOOL PersistentArrayMap_equals(PersistentArrayMap *self, PersistentArrayMap *other) {
  if(self->count != other->count) return FALSE;
  for(int i=0; i< HASHTABLE_THRESHOLD; i++) {
    void *key = self->keys[i];
    if(key == NULL) continue;
    void *val = self->values[i];
    void *otherVal = PersistentArrayMap_get(other, key);
    if(!equals(otherVal, val)) return FALSE;
  }
  return TRUE;
}

/* outside refcount system */
uint64_t PersistentArrayMap_hash(PersistentArrayMap *self) {
    uint64_t h = 5381;
    for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      void *key = self->keys[i];
      if(key == NULL) continue;
      h += (hash(key) ^ hash(self->values[i]));
    }
    return h;
}

/* mem done */
String *PersistentArrayMap_toString(PersistentArrayMap *self) {
  String *retVal = String_create("{");
  String *space = String_create(" ");
  String *closing = String_create("}");

  BOOL hasAtLeastOne = FALSE;
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      void *key = self->keys[i];
      if(key == NULL) continue;
      if(hasAtLeastOne) {
        retain(space);
        retVal = String_concat(retVal, space);
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
  return retVal;
}

/* outside refcount system */
void PersistentArrayMap_destroy(PersistentArrayMap *self, BOOL deallocateChildren) {
  if(deallocateChildren) {
    for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      void *key = self->keys[i];
      if(key) { 
        void *value = self->values[i];
        release(key); 
        release(value); 
      }      
    }
  }
}

/* outside refcount system */
void PersistentArrayMap_retainChildren(PersistentArrayMap *self, int except) {
 for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
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
    assert(self->count < 8 && "Maps of size > 8 not supported yet");
    int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
    PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
    retVal->count++;
    int foundPtr = -1;
    for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
      void *foundKey = retVal->keys[ptr];
      if(!foundKey) {
        retVal->values[ptr] = value;
        retVal->keys[ptr] = key;
        foundPtr = ptr;
        break;
      }
    }
    if(!reusable) PersistentArrayMap_retainChildren(retVal, foundPtr);
    return retVal;
  }
  
  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  if(reusable) release(retVal->values[found]);
  retVal->values[found] = value;
  if(!reusable) PersistentArrayMap_retainChildren(retVal, found);
  release(key);
  return retVal;
}

/* mem done */
PersistentArrayMap* PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key) {
  retain(self);
  retain(key);
  int64_t found = PersistentArrayMap_indexOf(self, key);
  if(found == -1) {
    release(key);
    return self;
  }
  BOOL reusable = isReusable(self);
  PersistentArrayMap *retVal = reusable ? self : PersistentArrayMap_copy(self);
  void *k = retVal->keys[found];
  void *v = retVal->values[found];
  retVal->keys[found] = NULL;
  retVal->values[found] = NULL;
  retVal->count--;
  if(!reusable) PersistentArrayMap_retainChildren(retVal, -1);
  else {
    release(k);
    release(v);
  }
  release(key);
  return retVal;
}

/* mem done */
int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key) {
 int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
    int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
    void *foundKey = self->keys[ptr];
    if(!foundKey) { 
      release(self);
      release(key);
      return -1;
    }
    if(equals(foundKey, key)) {
      release(self);
      release(key);
      return ptr;
    }
  }
  release(self);
  release(key);  
  return -1;
}

/* mem done */
void* PersistentArrayMap_get(PersistentArrayMap *self, void *key) {
  int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
    int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
    void *foundKey = self->keys[ptr];
    if(!foundKey) {      
      release(self);
      release(key);
      return Nil_create();
    }
    if(equals(foundKey, key)) {      
      void *foundValue = self->values[ptr];
      retain(foundValue);
      release(self);
      release(key);
      return foundValue;
    }
  }
  release(self);
  release(key);      
  return Nil_create();
}

/* mem done */
void* PersistentArrayMap_dynamic_get(void *self, void *key) {
  Object *type = super(self);
  if(type->type != persistentArrayMapType) { 
    release(self);
    release(key);    
    return Nil_create();
  }
  return PersistentArrayMap_get(self, key);
}

