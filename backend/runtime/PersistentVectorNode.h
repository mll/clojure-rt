#ifndef RT_PERSISTENT_VECTOR_NODE
#define RT_PERSISTENT_VECTOR_NODE
#include "String.h"

typedef enum {leafNode, internalNode} NodeType;

typedef struct PersistentVectorNode PersistentVectorNode;

struct PersistentVectorNode {
  NodeType type;
  uint64_t count;
  Object *array[];
};

PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type);
BOOL PersistentVectorNode_equals(PersistentVectorNode *self, PersistentVectorNode *other);
uint64_t PersistentVectorNode_hash(PersistentVectorNode *self);
String *PersistentVectorNode_toString(PersistentVectorNode *self);
void PersistentVectorNode_destroy(PersistentVectorNode *self, BOOL deallocateChildren);

PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode *self, uint64_t level, uint64_t index, Object *other);
PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode *parent, PersistentVectorNode *self, PersistentVectorNode *tailToPush, int32_t level, BOOL *copied);
#endif
