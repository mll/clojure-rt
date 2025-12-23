#ifndef RT_CONCURRENT_HASH_MAP
#define RT_CONCURRENT_HASH_MAP
#include <stdatomic.h>
#include "defines.h"
#include "String.h"
#include "RTValue.h"

/* https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/
   Leapfrog */

typedef struct Object Object;

#define CHM_REDIRECT 1
#define CHM_EMPTY 0

typedef struct ConcurrentHashMapEntry {
  RTValue _Atomic key;
  RTValue _Atomic value;
  _Atomic uword_t keyHash;
  _Atomic unsigned short leaps;
} ConcurrentHashMapEntry;

typedef struct ConcurrentHashMapNode {
  uword_t sizeMask;
  short int resizingThreshold;
  ConcurrentHashMapEntry array[];
} ConcurrentHashMapNode;

typedef struct ConcurrentHashMap {
  Object super;
  ConcurrentHashMapNode * _Atomic root;
} ConcurrentHashMap;


ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent);

void ConcurrentHashMap_assoc(ConcurrentHashMap *self, RTValue key, RTValue value);
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, RTValue key);
RTValue ConcurrentHashMap_get(ConcurrentHashMap *self, RTValue key);

bool ConcurrentHashMap_equals(ConcurrentHashMap *self, ConcurrentHashMap *other);
uword_t ConcurrentHashMap_hash(ConcurrentHashMap *self);
String *ConcurrentHashMap_toString(ConcurrentHashMap *self);
void ConcurrentHashMap_destroy(ConcurrentHashMap *self);

#endif
