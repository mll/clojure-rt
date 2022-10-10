#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#include "Object.h"

typedef struct PersistentVector PersistentVector;
typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct PersistentVectorLeafNode PersistentVectorLeafNode;

struct PersistentVector {
  uint32_t count;
  uint32_t shift;
  uint32_t tail_len;
  PersistentVectorLeafNode *tail;
  PersistentVectorNode *root;
};

PersistentVector* PersistentVector_create();

bool PersistentVector_equals(PersistentVector *self, PersistentVector *other);
uint64_t PersistentVector_hash(PersistentVector *self);
String *PersistentVector_toString(PersistentVector *self);
void PersistentVector_destroy(PersistentVector *self, bool deallocateChildren);

PersistentVector* PersistentVector_conj(PersistentVector *self, Object *other);

bool PersistentVectorNode_equals(PersistentVectorNode *self, PersistentVectorNode *other);
uint64_t PersistentVectorNode_hash(PersistentVectorNode *self);
String *PersistentVectorNode_toString(PersistentVectorNode *self);
void PersistentVectorNode_destroy(PersistentVectorNode *self, bool deallocateChildren);

#endif
