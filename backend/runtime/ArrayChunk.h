#ifndef RT_ARRAY_CHUNK_H
#define RT_ARRAY_CHUNK_H

#include "ObjectProto.h"
#include "RTValue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PersistentVectorNode PersistentVectorNode;

struct ArrayChunk {
  Object super;
  PersistentVectorNode *node;
  uword_t offset;
};

typedef struct ArrayChunk ArrayChunk;

ArrayChunk *ArrayChunk_create(PersistentVectorNode *node, uword_t offset);
void ArrayChunk_destroy(ArrayChunk *self, bool deallocateChildren);
RTValue ArrayChunk_nth(ArrayChunk *self, int32_t i);
RTValue ArrayChunk_nth_default(ArrayChunk *self, int32_t i, RTValue notFound);
int32_t ArrayChunk_count(ArrayChunk *self);

#ifdef __cplusplus
}
#endif

#endif
