#include "ArrayChunk.h"
#include "Object.h"
#include "PersistentVectorNode.h"
#include "Exceptions.h"

ArrayChunk *ArrayChunk_create(PersistentVectorNode *node, uword_t offset) {
  ArrayChunk *self = (ArrayChunk *)allocate(sizeof(ArrayChunk));
  Object_create((Object *)self, arrayChunkType);
  self->node = node;
  self->offset = offset;
  Ptr_retain(node);
  return self;
}

void ArrayChunk_destroy(ArrayChunk *self, bool deallocateChildren) {
  Ptr_release(self->node);
}

RTValue ArrayChunk_nth(ArrayChunk *self, int32_t i) {
  uword_t idx = self->offset + i;
  if (idx >= self->node->count) {
    uword_t cnt = self->node->count - self->offset;
    Ptr_release(self);
    throwIndexOutOfBoundsException_C(i, cnt);
  }
  RTValue val = self->node->array[idx];
  retain(val);
  Ptr_release(self);
  return val;
}

RTValue ArrayChunk_nth_default(ArrayChunk *self, int32_t i, RTValue notFound) {
  uword_t idx = self->offset + i;
  if (idx >= self->node->count) {
    Ptr_release(self);
    return notFound;
  }
  RTValue val = self->node->array[idx];
  retain(val);
  release(notFound);
  Ptr_release(self);
  return val;
}

int32_t ArrayChunk_count(ArrayChunk *self) {
  int32_t res = (int32_t)(self->node->count - self->offset);
  Ptr_release(self);
  return res;
}
