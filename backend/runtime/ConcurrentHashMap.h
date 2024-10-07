#ifndef RT_CONCURRENT_HASH_MAP
#define RT_CONCURRENT_HASH_MAP
#include <stdatomic.h>
#include "defines.h"
#include "String.h"

/* https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/
   Leapfrog */

typedef struct Object Object;

#define REDIRECT 1
#define EMPTY 0

typedef struct ConcurrentHashMapEntry {
  Object * _Atomic key;
  Object * _Atomic value;
  _Atomic uint64_t keyHash;
  _Atomic unsigned short leaps;
} ConcurrentHashMapEntry;

typedef struct ConcurrentHashMapNode {
  uint64_t sizeMask;
  short int resizingThreshold;
  ConcurrentHashMapEntry array[];
} ConcurrentHashMapNode;

typedef struct ConcurrentHashMap {
 ConcurrentHashMapNode * _Atomic root;
} ConcurrentHashMap;


ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent);

void ConcurrentHashMap_assoc(ConcurrentHashMap *self, void *key, void *value);
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, void *key);
void *ConcurrentHashMap_get(ConcurrentHashMap *self, void *key);

BOOL ConcurrentHashMap_equals(ConcurrentHashMap *self, ConcurrentHashMap *other);
uint64_t ConcurrentHashMap_hash(ConcurrentHashMap *self);
String *ConcurrentHashMap_toString(ConcurrentHashMap *self);
void ConcurrentHashMap_destroy(ConcurrentHashMap *self);

#endif
