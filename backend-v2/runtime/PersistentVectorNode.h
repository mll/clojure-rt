#ifndef RT_PERSISTENT_VECTOR_NODE
#define RT_PERSISTENT_VECTOR_NODE

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"

typedef enum { leafNode, internalNode } NodeType;

typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct String String;

struct PersistentVectorNode {
  Object super;
  NodeType type;
  uword_t count;
  uword_t transientID;
  RTValue array[];
};

PersistentVectorNode *PersistentVectorNode_allocate(uword_t count,
                                                    NodeType type,
                                                    uword_t transientID);
bool PersistentVectorNode_equals(PersistentVectorNode *restrict self,
                                 PersistentVectorNode *restrict other);
uword_t PersistentVectorNode_hash(PersistentVectorNode *restrict self);
String *PersistentVectorNode_toString(PersistentVectorNode *restrict self);
void PersistentVectorNode_destroy(PersistentVectorNode *restrict self,
                                  bool deallocateChildren);

PersistentVectorNode *PersistentVectorNode_pushTail(
    PersistentVectorNode *restrict parent, PersistentVectorNode *restrict self,
    PersistentVectorNode *restrict tailToPush, int32_t level, bool *copied,
    bool allowsReuse, uword_t vectorTransientID);
PersistentVectorNode *
PersistentVectorNode_replacePath(PersistentVectorNode *restrict self,
                                 uword_t level, uword_t index, RTValue other,
                                 bool allowsReuse, uword_t vectorTransientID);
PersistentVectorNode *
PersistentVectorNode_popTail(PersistentVectorNode *restrict self,
                             PersistentVectorNode **restrict poppedLeaf,
                             bool allowsReuse, uword_t vectorTransientID);

bool PersistentVectorNode_contains(PersistentVectorNode *restrict self,
                                   RTValue other);

#ifdef __cplusplus
}
#endif

#endif
