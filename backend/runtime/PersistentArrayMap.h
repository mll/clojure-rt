#ifndef RT_PERSISTENT_ARRAY_MAP
#define RT_PERSISTENT_ARRAY_MAP

#include "String.h"

typedef struct Object Object;
typedef struct PersistentArrayMap PersistentArrayMap;

struct PersistentArrayMap {
  uint64_t count;
  void *keys[HASHTABLE_THRESHOLD];
  void *values[HASHTABLE_THRESHOLD];
};

PersistentArrayMap* PersistentArrayMap_empty();

BOOL PersistentArrayMap_equals(PersistentArrayMap *self, PersistentArrayMap *other);
uint64_t PersistentArrayMap_hash(PersistentArrayMap *self);
String *PersistentArrayMap_toString(PersistentArrayMap *self);
void PersistentArrayMap_destroy(PersistentArrayMap *self, BOOL deallocateChildren);

PersistentArrayMap* PersistentArrayMap_assoc(PersistentArrayMap *self, void *key, void *value);
PersistentArrayMap* PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key);
void* PersistentArrayMap_get(PersistentArrayMap *self, void *key);
void* PersistentArrayMap_dynamic_get(void *self, void *key);
int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key);

PersistentArrayMap* PersistentArrayMap_create();
PersistentArrayMap* PersistentArrayMap_createMany(uint64_t pairCount, ...);

#endif
