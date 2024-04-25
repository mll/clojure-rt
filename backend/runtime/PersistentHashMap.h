#ifndef RT_PERSISTENT_ARRAY_MAP
#define RT_PERSISTENT_ARRAY_MAP

#define HashMapNode Object

#include "String.h"
#include "Object.h"

// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;
typedef struct PersistentArrayMap PersistentArrayMap;

struct PersistentArrayMap {
    uint64_t count;
    HashMapNode *root;
};

struct BitmapIndexedNode {
    uint32_t bitmap;
    Object *array[];
};

struct HashCollisionNode {
    uint32_t hash;
    uint32_t count;
    Object *array[];
};

struct ContainerNode {
    uint32_t count;
    Object *array[];
};

typedef struct BitmapIndexedNode BitmapIndexedNode;
typedef struct HashCollisionNode HashCollisionNode;
typedef struct ContainerNode ContainerNode;


/* outside refcount system */
inline uint32_t BitmapIndexedNode_index(BitmapIndexedNode *self, uint32_t bit) {
    return __builtin_popcount(self->bitmap & (bit - 1));
}
inline uint32_t BitmapIndexedNode_mask(uint32_t hash, uint32_t shift) {
    return (hash >> shift) & 0x01f;
}
inline uint32_t BitmapIndexedNode_bitpos(uint32_t hash, uint32_t shift) {
    return 1 << BitmapIndexedNode_mask(hash, shift);
}

inline BitmapIndexedNode *BitmapIndexedNode_cloneAndSet(BitmapIndexedNode *nodeWithArray, uint32_t idx, HashMapNode* a) {
    uint32_t arraySize = __builtin_popcount(nodeWithArray->bitmap);
    BitmapIndexedNode *clone = allocate(sizeof(BitmapIndexedNode) * arraySize);
    memcpy(clone->array, nodeWithArray->array, sizeof(BitmapIndexedNode) * arraySize);

    for (int i=0; i<arraySize; i++) {
        if (clone->array[i] == NULL || i == idx) {
            continue;
        }
        retain(clone->array[i]);
    }


    clone->array[idx] = a;
    clone->bitmap = nodeWithArray->bitmap;

    release(nodeWithArray);
    return clone;
}

inline BitmapIndexedNode *BitmapIndexedNode_cloneAndSet(BitmapIndexedNode *nodeWithArray, uint32_t idx, HashMapNode* a, uint32_t idx2, HashMapNode* a2) {
    uint32_t arraySize = __builtin_popcount(nodeWithArray->bitmap);
    BitmapIndexedNode *clone = allocate(sizeof(BitmapIndexedNode) * arraySize);
    memcpy(clone->array, nodeWithArray->array, sizeof(BitmapIndexedNode) * arraySize);

    for (int i=0; i<arraySize; i++) {
        if (clone->array[i] == NULL || i == idx || i == idx2) {
            continue;
        }
        retain(clone->array[i]);
    }

    clone->array[idx] = a;
    clone->array[idx2] = a2;
    clone->bitmap = nodeWithArray->bitmap;

    release(nodeWithArray);
    return clone;
}

HashMapNode* PersistentHashMap_createNode(uint32_t shift, void *key1, void *val1, uint32_t hash2, void *key2, void *val2, BOOL *isNodeAdded);



HashMapNode* BitmapIndexedNode_assoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key, void *value, BOOL *isNodeAdded);
HashMapNode* BitmapIndexedNode_dissoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key);

BitmapIndexedNode *BitmapIndexedNode_create(uint32_t bitmap, uint32_t arraySize, HashMapNode* *array);
BitmapIndexedNode *BitmapIndexedNode_createWithout(BitmapIndexedNode *node, uint32_t bitmap, uint32_t idx);
BitmapIndexedNode *BitmapIndexedNode_createWithInsertion(uint32_t bitmap, uint32_t arraySize, HashMapNode* *oldArray, uint32_t insertIndex, void *keyToInsert, void *valueToInsert);
BitmapIndexedNode *BitmapIndexedNode_empty();

HashCollisionNode *HashCollisionNode_create(uint32_t hash, uint32_t count, Object *array[]);
HashCollisionNode *HashCollisionNode_cloneAndSet(HashCollisionNode *nodeWithArray, uint32_t idx, void *key, void *value);
HashCollisionNode *HashCollisionNode_assoc(HashCollisionNode *self, uint32_t shift, uint32_t hash, void *key, void *value, BOOL *isNodeAdded);
HashCollisionNode *HashCollisionNode_dissoc(HashCollisionNode *self, uint32_t hash, void *key);

ContainerNode *ContainerNode_cloneAndSet(ContainerNode *nodeWithArray, uint32_t idx, HashMapNode* a);
BitmapIndexedNode *ContainerNode_pack(ContainerNode *node, uint32_t index);

ContainerNode *ContainerNode_create(uint32_t count, Object *array[]);
ContainerNode *ContainerNode_createWithInsertion(ContainerNode *self, uint32_t idx, void *key, void *value);
ContainerNode *ContainerNode_createFromBitmapIndexedNode(BitmapIndexedNode *node, uint32_t shift, uint32_t insertHash, uint32_t insertIndex, void *keyToInsert, void *valueToInsert, BOOL *isNodeAdded);

ContainerNode *ContainerNode_assoc(ContainerNode *self, uint32_t shift, uint32_t hash, void *key, void *value, BOOL *isNodeAdded);
ContainerNode *ContainerNode_dissoc(ContainerNode *self, uint32_t shift, uint32_t hash, void *key);


Object *BitmapIndexedNode_get(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, void *key);

HashMapNode *PersistentHashMapNode_assoc(HashMapNode *self, uint32_t shift, uint32_t hash, void *key, void *value);
HashMapNode *PersistentHashMapNode_dissoc(HashMapNode *self, uint32_t shift, uint32_t hash, void *key);
Object *PersistentHashMapNode_get(HashMapNode *self, uint32_t shift, uint32_t hash, void *key);



PersistentArrayMap* PersistentArrayMap_empty();

BOOL PersistentArrayMap_equals(PersistentArrayMap *self, PersistentArrayMap *other);
uint64_t PersistentArrayMap_hash(PersistentArrayMap *self);
String *PersistentArrayMap_toString(PersistentArrayMap *self);
void PersistentArrayMap_destroy(PersistentArrayMap *self, BOOL deallocateChildren);



PersistentArrayMap* PersistentArrayMap_assoc(PersistentArrayMap *self, void *key, void *value);
PersistentArrayMap *PersistentArrayMap_copy(PersistentArrayMap *self);
PersistentArrayMap* PersistentArrayMap_dissoc(PersistentArrayMap *self, void *key);
void* PersistentArrayMap_get(PersistentArrayMap *self, void *key);
void* PersistentArrayMap_dynamic_get(void *self, void *key);
int64_t PersistentArrayMap_indexOf(PersistentArrayMap *self, void *key);

PersistentArrayMap* PersistentArrayMap_create();
PersistentArrayMap* PersistentArrayMap_createMany(uint64_t pairCount, ...);

#endif
