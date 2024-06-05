#ifndef RT_PERSISTENT_HASH_MAP
#define RT_PERSISTENT_HASH_MAP

#define HashMapNode Object

#include <stdint.h>
#include "String.h"
#include "Object.h"

// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;
typedef struct PersistentHashMap PersistentHashMap;

struct PersistentHashMap {
    uint64_t count;
    Object *root;
    Object *nilValue;
};

Object* PersistentHashMap_createNode(uint32_t shift, Object * key1, Object * val1, uint32_t hash2, Object * key2, Object * val2, BOOL *isNodeAdded);

Object* PersistentHashMapNode_assoc(Object *self, uint32_t shift, uint32_t hash, Object * key, Object * value, BOOL *isNodeAdded);
PersistentHashMap *PersistentHashMapNode_dissoc(Object *self, uint32_t shift, uint32_t hash, void *key);
Object *PersistentHashMapNode_get(Object *self, uint32_t shift, uint32_t hash, void *key);



PersistentHashMap* PersistentHashMap_empty();

BOOL PersistentHashMap_equals(PersistentHashMap * self, PersistentHashMap * other);
uint64_t PersistentHashMap_hash(PersistentHashMap *self);
String *PersistentHashMap_toString(PersistentHashMap *self);
void PersistentHashMap_destroy(PersistentHashMap *self, BOOL deallocateChildren);
void PersistentHashMapNode_check(Object *child, uint32_t expectedRefCount);
void PersistentHashMap_childrenCheck(PersistentHashMap *map, uint32_t expectedRefCount);



PersistentHashMap* PersistentHashMap_assoc(PersistentHashMap *self, Object *key, Object *value);
PersistentHashMap *PersistentHashMap_copy(PersistentHashMap *self);
PersistentHashMap* PersistentHashMap_dissoc(PersistentHashMap *self, void *key);
void* PersistentHashMap_get(PersistentHashMap *self, void *key);
void* PersistentHashMap_dynamic_get(void *self, void *key);
int64_t PersistentHashMap_indexOf(PersistentHashMap *self, void *key);

PersistentHashMap* PersistentHashMap_create();
PersistentHashMap* PersistentHashMap_createMany(uint64_t pairCount, ...);

#endif
