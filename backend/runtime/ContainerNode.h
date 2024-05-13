#ifndef RT_CONTAINER_NODE
#define RT_CONTAINER_NODE

#define HashMapNode Object

#include "String.h"
#include "Object.h"
#include "BitmapIndexedNode.h"
#include <stdint.h>

// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;

struct ContainerNode {
    uint32_t count;
    Object *array[];
};

typedef struct ContainerNode ContainerNode;
typedef struct BitmapIndexedNode BitmapIndexedNode;

ContainerNode *ContainerNode_cloneAndSet(ContainerNode *nodeWithArray, uint32_t idx, HashMapNode* a);
BitmapIndexedNode *ContainerNode_pack(ContainerNode *node, uint32_t index);

ContainerNode *ContainerNode_create(uint32_t count, Object *array[]);
ContainerNode *ContainerNode_createWithInsertion(ContainerNode *self, uint32_t idx, void *key, void *value);
Object *ContainerNode_createFromBitmapIndexedNode(BitmapIndexedNode *node, uint32_t shift, uint32_t insertHash, uint32_t insertIndex, Object *keyToInsert, Object *valueToInsert, BOOL *isNodeAdded);

Object *ContainerNode_assoc(ContainerNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value, BOOL *isNodeAdded);
ContainerNode *ContainerNode_dissoc(ContainerNode *self, uint32_t shift, uint32_t hash, void *key);


BOOL ContainerNode_equals(ContainerNode *self, ContainerNode *other);
uint64_t ContainerNode_hash(ContainerNode *self);
String *ContainerNode_toString(ContainerNode *self);
void ContainerNode_destroy(ContainerNode *self, BOOL deallocateChildren);

#endif
