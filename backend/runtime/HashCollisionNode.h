#ifndef RT_HASH_COLLISION_NODE
#define RT_HASH_COLLISION_NODE

#define HashMapNode Object

#include "String.h"
#include "Object.h"
#include <stdint.h>

// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;

struct HashCollisionNode {
    uint32_t hash;
    uint32_t count;
    Object *array[];
};

typedef struct HashCollisionNode HashCollisionNode;


HashCollisionNode *HashCollisionNode_create(uint32_t hash, uint32_t count, ...);
HashCollisionNode *HashCollisionNode_cloneAndSet(HashCollisionNode *nodeWithArray, uint32_t idx, Object *key, Object *value);
Object *HashCollisionNode_assoc(HashCollisionNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value, BOOL *isNodeAdded);
HashCollisionNode *HashCollisionNode_dissoc(HashCollisionNode *self, uint32_t hash, Object *key);

void HashCollisionNode_destroy(HashCollisionNode *self, BOOL deallocateChildren);
uint64_t HashCollisionNode_hash(HashCollisionNode *self);
BOOL HashCollisionNode_equals(HashCollisionNode *self, HashCollisionNode *other);
String *HashCollisionNode_toString(HashCollisionNode *self);

#endif
