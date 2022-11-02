#ifndef RT_CONCURRENT_HASH_MAP
#define RT_CONCURRENT_HASH_MAP
#include <stdatomic.h>
#include "defines.h"
#include "String.h"


typedef struct Object Object;

typedef struct ConcurrentHashMapEntry {
  Object * _Atomic key;
  Object * _Atomic value;
  _Atomic uint64_t keyHash;
  _Atomic unsigned short leaps;
} ConcurrentHashMapEntry;

typedef struct ConcurrentHashMapNode {
  uint64_t sizeMask;
  ConcurrentHashMapEntry array[];
} ConcurrentHashMapNode;

typedef struct ConcurrentHashMap {
 ConcurrentHashMapNode * _Atomic root;
} ConcurrentHashMap;

ConcurrentHashMap *ConcurrentHashMap_create();

void ConcurrentHashMap_assoc(ConcurrentHashMap *self, Object *key, Object *value);
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, Object *key);
Object *ConcurrentHashMap_get(ConcurrentHashMap *self, Object *key);

BOOL ConcurrentHashMap_equals(ConcurrentHashMap *self, ConcurrentHashMap *other);
uint64_t ConcurrentHashMap_hash(ConcurrentHashMap *self);
String *ConcurrentHashMap_toString(ConcurrentHashMap *self);
void ConcurrentHashMap_destroy(ConcurrentHashMap *self);

#endif
