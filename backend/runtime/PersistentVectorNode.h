#ifndef RT_PERSISTENT_VECTOR_NODE
#define RT_PERSISTENT_VECTOR_NODE
#include "String.h"
#include "PersistentVector.h"

typedef enum {leafNode, internalNode} NodeType;

typedef struct PersistentVectorNode PersistentVectorNode;

struct PersistentVectorNode {
  NodeType type;
  uint64_t count;
  void *array[RRB_BRANCHING];
};

PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type);
BOOL PersistentVectorNode_equals(PersistentVectorNode * restrict self, PersistentVectorNode * restrict other);
uint64_t PersistentVectorNode_hash(PersistentVectorNode * restrict self);
String *PersistentVectorNode_toString(PersistentVectorNode * restrict self);
void PersistentVectorNode_destroy(PersistentVectorNode * restrict self, BOOL deallocateChildren);

PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode * restrict self, uint64_t level, uint64_t index, void * restrict other);
PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode * restrict parent, PersistentVectorNode * restrict self, PersistentVectorNode * restrict tailToPush, int32_t level, BOOL *copied);
#endif
