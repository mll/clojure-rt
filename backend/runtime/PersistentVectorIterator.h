#ifndef RT_PERSISTENT_VECTOR_ITERATOR
#define RT_PERSISTENT_VECTOR_ITERATOR
#include "ObjectProto.h"
/* #include "PersistentVectorNode.h" */
/* #include "PersistentVector.h" */
#include <stdint.h>

typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct PersistentVector PersistentVector;

struct PersistentVectorIterator {
  uint64_t index;
  uint64_t blockIndex;
  PersistentVectorNode *block;
  PersistentVector *parent;
};

typedef struct PersistentVectorIterator PersistentVectorIterator;

PersistentVectorIterator PersistentVector_iterator(PersistentVector *self);
Object *PersistentVector_iteratorGet(PersistentVectorIterator *it);
Object *PersistentVector_iteratorNext(PersistentVectorIterator *it);

#endif
