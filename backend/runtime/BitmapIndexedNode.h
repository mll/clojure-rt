#ifndef RT_BITMAP_INDEXED_NODE
#define RT_BITMAP_INDEXED_NODE

#define HashMapNode Object

#include "String.h"
#include "Object.h"
#include <stdint.h>
#include "ContainerNode.h"

// http://blog.higher-order.net/2009/09/08/understanding-clojures-persistenthashmap-deftwice
// https://groups.google.com/g/clojure/c/o12kFA-j1HA
// https://gist.github.com/mrange/d6e7415113ebfa52ccb660f4ce534dd4
// https://www.infoq.com/articles/in-depth-look-clojure-collections/

typedef struct Object Object;

struct BitmapIndexedNode {
  uint32_t bitmap;
  Object *array[];
};

typedef struct BitmapIndexedNode BitmapIndexedNode;

uint32_t BitmapIndexedNode_index(BitmapIndexedNode *self, uint32_t bit);
uint32_t BitmapIndexedNode_mask(uint32_t hash, uint32_t shift);
uint32_t BitmapIndexedNode_bitpos(uint32_t hash, uint32_t shift);

BitmapIndexedNode *BitmapIndexedNode_cloneAndSet(BitmapIndexedNode *nodeWithArray, uint32_t idx, HashMapNode* a);

BitmapIndexedNode *BitmapIndexedNode_cloneAndSetTwo(BitmapIndexedNode *nodeWithArray, uint32_t idx, Object * a, uint32_t idx2, Object * a2);



Object* BitmapIndexedNode_assoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key, Object *value, BOOL *isNodeAdded);
HashMapNode* BitmapIndexedNode_dissoc(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key);

BitmapIndexedNode *BitmapIndexedNode_create();
BitmapIndexedNode *BitmapIndexedNode_createVa(uint32_t bitmap, uint32_t arraySize, ...);
BitmapIndexedNode *BitmapIndexedNode_createWithout(BitmapIndexedNode *node, uint32_t bitmap, uint32_t idx);
BitmapIndexedNode *BitmapIndexedNode_createWithInsertion(BitmapIndexedNode *oldSelf, uint32_t arraySize, uint32_t insertIndex, Object *keyToInsert, Object *valueToInsert, uint32_t oldBitmap);
BitmapIndexedNode *BitmapIndexedNode_empty();


Object *BitmapIndexedNode_get(BitmapIndexedNode *self, uint32_t shift, uint32_t hash, Object *key);

BOOL BitmapIndexedNode_equals(BitmapIndexedNode *self, BitmapIndexedNode *other);
uint64_t BitmapIndexedNode_hash(BitmapIndexedNode *self);
String *BitmapIndexedNode_toString(BitmapIndexedNode *self);
void BitmapIndexedNode_destroy(BitmapIndexedNode *self, BOOL deallocateChildren);


#endif
