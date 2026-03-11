#ifndef RT_PERSISTENT_VECTOR_ITERATOR
#define RT_PERSISTENT_VECTOR_ITERATOR

#ifdef __cplusplus
extern "C" {
#endif

#include "ObjectProto.h"
/* #include "PersistentVectorNode.h" */
/* #include "PersistentVector.h" */
#include "RTValue.h"
#include <stdint.h>

typedef struct PersistentVectorNode PersistentVectorNode;
typedef struct PersistentVector PersistentVector;

struct PersistentVectorIterator {
  uword_t index;
  uword_t blockIndex;
  PersistentVectorNode *block;
  PersistentVector *parent;
};

typedef struct PersistentVectorIterator PersistentVectorIterator;

PersistentVectorIterator PersistentVector_iterator(PersistentVector *self);
RTValue PersistentVector_iteratorGet(PersistentVectorIterator *it);
RTValue PersistentVector_iteratorNext(PersistentVectorIterator *it);

#ifdef __cplusplus
}
#endif

#endif
