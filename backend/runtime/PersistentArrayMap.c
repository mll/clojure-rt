#include "Object.h"
#include "PersistentArrayMap.h"
#include "Nil.h"
#include <stdarg.h>
#include "defines.h"

static PersistentArrayMap *EMPTY = NULL;

PersistentArrayMap* PersistentArrayMap_empty() {
  if (EMPTY == NULL) EMPTY = PersistentArrayMap_create();
  retain(EMPTY);
  return EMPTY;
} 

PersistentArrayMap* PersistentArrayMap_create() {
  size_t size = sizeof(PersistentArrayMap) + sizeof(Object);
  Object *super = allocate(size); 
  PersistentArrayMap *self = Object_data(super);
  memset(super, 0, size);
  Object_create(super, persistentArrayMapType);
  return self;
}

PersistentArrayMap* PersistentArrayMap_copy(PersistentArrayMap *other) {
  size_t baseSize = sizeof(PersistentArrayMap);
  size_t size = baseSize + sizeof(Object);
  Object *super = allocate(size); 
  PersistentArrayMap *self = Object_data(super);
  memcpy(self, other, baseSize);
  Object_create(super, persistentArrayMapType);
  return self;
}

PersistentArrayMap* PersistentArrayMap_createMany(uint64_t pairCount, ...) {
  va_list args;
  va_start(args, pairCount);

  PersistentArrayMap *self = PersistentArrayMap_create();

  self->count = pairCount;
  for(int i=0; i<pairCount; i++) {
    void *k = va_arg(args, void *);
    void *v = va_arg(args, void *);
    self->keys[i] = k;
    self->values[i] = v;
    // TODO new memory management strategy will get rid of this: 
    retain(k);
    retain(v);
  }
  va_end(args);
  return self;
}

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

uint64_t PersistentArrayMap_hash(PersistentArrayMap *self) {
    uint64_t h = 5381;
    for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      void *key = self->keys[i];
      if(key == NULL) continue;
      h += (hash(key) ^ hash(self->values[i]));
    }
    return h;
}

String *PersistentArrayMap_toString(PersistentArrayMap *self) {
  String *retVal = String_create("{");
  String *space = String_create(" ");
  String *closing = String_create("}");

  BOOL hasAtLeastOne = FALSE;
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      void *key = self->keys[i];
      if(key == NULL) continue;
      if(hasAtLeastOne) retVal = String_append(retVal, space);
      hasAtLeastOne = TRUE;
      String *s = toString(key);
      retVal = String_append(retVal, s);
      release(s);
      retVal = String_append(retVal, space);
      s = toString(self->values[i]);
      retVal = String_append(retVal, s);
      release(s);
  }

  retVal = String_append(retVal, closing); 
  release(space);
  release(closing);
  return retVal;
}

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

void PersistentArrayMap_retainChildren(PersistentArrayMap *self) {
 for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
    void *foundKey = self->keys[i];
    if(foundKey) {
      retain(self->keys[i]);
      retain(self->values[i]);
    }
 }
}

PersistentArrayMap* PersistentArrayMap_assoc(PersistentArrayMap *self, void *key, void *value) {
  int64_t found = PersistentArrayMap_indexOf(self, key);
  if(found == -1) {
    assert(self->count < 8 && "Maps of size > 8 not suppoetrd yet");
    int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
    PersistentArrayMap *retVal = PersistentArrayMap_copy(self);
    retVal->count++;
    for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
      int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
      void *foundKey = retVal->keys[ptr];
      if(!foundKey) {
        retVal->values[ptr] = value;
        retVal->keys[ptr] = key;
        break;
      }
    }
    PersistentArrayMap_retainChildren(retVal);
    return retVal;
  }
  
  PersistentArrayMap *retVal = PersistentArrayMap_copy(self);
  retVal->values[found] = value;
  PersistentArrayMap_retainChildren(retVal);
  return retVal;
}

PersistentArrayMap* PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key) {
  int64_t found = PersistentArrayMap_indexOf(self, key);
  if(found == -1) { 
    retain(self);
    return self;
  }

  PersistentArrayMap *retVal = PersistentArrayMap_copy(self);
  retVal->keys[found] = NULL;
  retVal->values[found] = NULL;
  retVal->count--;
  PersistentArrayMap_retainChildren(retVal);
  return retVal;
}

int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key) {
 int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
    int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
    void *foundKey = self->keys[ptr];
    if(!foundKey) return -1;
    if(equals(foundKey, key)) return ptr;
  }
  return -1;
}


void* PersistentArrayMap_get(PersistentArrayMap *self, void *key) {
  int64_t idx = hash(key) & (HASHTABLE_THRESHOLD - 1);
  for(int i=0; i<HASHTABLE_THRESHOLD; i++) {
    int64_t ptr = (idx + i) & (HASHTABLE_THRESHOLD - 1);
    void *foundKey = self->keys[ptr];
    if(!foundKey) return Nil_create();
    if(equals(foundKey, key)) {
      void *foundValue = self->values[ptr];
      retain(foundValue);
      return foundValue;
    }
  }
  return Nil_create();
}

void* PersistentArrayMap_dynamic_get(void *self, void *key) {
  Object *type = super(self);
  if(type->type != persistentArrayMapType) return Nil_create();
  return PersistentArrayMap_get(self, key);
}

