#ifndef RT_PERSISTENT_VECTOR_NODE
#define RT_PERSISTENT_VECTOR_NODE


typedef enum {leafNode, internalNode} NodeType;

typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct String String;

struct PersistentVectorNode {
  Object super;
  NodeType type;
  uint64_t count;
  uint64_t transientID;
  Object *array[];
};

PersistentVectorNode* PersistentVectorNode_allocate(uint64_t count, NodeType type, uint64_t transientID);
BOOL PersistentVectorNode_equals(PersistentVectorNode *  self, PersistentVectorNode *  other);
uint64_t PersistentVectorNode_hash(PersistentVectorNode *  self);
String *PersistentVectorNode_toString(PersistentVectorNode *  self);
void PersistentVectorNode_destroy(PersistentVectorNode *  self, BOOL deallocateChildren);

PersistentVectorNode *PersistentVectorNode_pushTail(PersistentVectorNode *  parent, PersistentVectorNode *  self, PersistentVectorNode *  tailToPush, int32_t level, BOOL *copied, BOOL allowsReuse, uint64_t vectorTransientID);
PersistentVectorNode *PersistentVectorNode_replacePath(PersistentVectorNode *  self, uint64_t level, uint64_t index, Object *  other, BOOL allowsReuse, uint64_t vectorTransientID);
PersistentVectorNode *PersistentVectorNode_popTail(PersistentVectorNode *  self, PersistentVectorNode **  poppedLeaf, BOOL allowsReuse, uint64_t vectorTransientID);

BOOL PersistentVectorNode_contains(PersistentVectorNode *  self, void *  other);
#endif
