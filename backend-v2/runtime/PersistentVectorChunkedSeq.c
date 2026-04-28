#include "PersistentVectorChunkedSeq.h"
#include "ArrayChunk.h"
#include "Object.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Exceptions.h"
#include <stdio.h>

PersistentVectorChunkedSeq *PersistentVectorChunkedSeq_create(PersistentVectorIterator it) {
  PersistentVectorChunkedSeq *self = (PersistentVectorChunkedSeq *)allocate(sizeof(PersistentVectorChunkedSeq));
  Object_create((Object *)self, persistentVectorChunkedSeqType);
  self->it = it;
  Ptr_retain(self->it.parent);
  return self;
}

void PersistentVectorChunkedSeq_destroy(PersistentVectorChunkedSeq *self, bool deallocateChildren) {
  Ptr_release(self->it.parent);
}

uint64_t PersistentVectorChunkedSeq_hash(PersistentVectorChunkedSeq *self) {
  // Simple hash for now, could be improved by iterating if needed,
  // but sequences usually hash based on contents.
  // For now let's use a placeholder or iterate.
  return 0xDEADBEEF ^ self->it.index;
}

bool PersistentVectorChunkedSeq_equals(PersistentVectorChunkedSeq *self, PersistentVectorChunkedSeq *other) {
  if (self == other)
    return true;
  if (self->it.parent != other->it.parent)
    return false;
  return self->it.index == other->it.index;
}

bool PersistentVectorChunkedSeq_equals_managed(PersistentVectorChunkedSeq *self, RTValue other) {
  bool res = false;
  if (RT_isPtr(other)) {
    Object *obj = (Object *)RT_unboxPtr(other);
    if (obj->type == persistentVectorChunkedSeqType) {
      res = PersistentVectorChunkedSeq_equals(self, (PersistentVectorChunkedSeq *)obj);
    }
  }
  Ptr_release(self);
  release(other);
  return res;
}

String *PersistentVectorChunkedSeq_toString(PersistentVectorChunkedSeq *self) {
  String *res = String_createStatic("(");
  PersistentVectorIterator it = self->it;
  bool first = true;

  while (it.index < it.parent->count) {
    if (!first) {
      res = String_concat(res, String_createStatic(" "));
    }
    RTValue val = PersistentVector_iteratorGet(&it);
    retain(val);
    String *s = toString(val); // toString consumes val
    res = String_concat(res, s); // String_concat consumes both res and s
    first = false;
    PersistentVector_iteratorNext(&it);
  }

  res = String_concat(res, String_createStatic(")"));
  Ptr_release(self);
  return res;
}

RTValue PersistentVectorChunkedSeq_first(PersistentVectorChunkedSeq *self) {
  if (self->it.index >= self->it.parent->count) {
    Ptr_release(self);
    return RT_boxNil();
  }
  RTValue val = PersistentVector_iteratorGet(&self->it);
  retain(val);
  Ptr_release(self);
  return val;
}

RTValue PersistentVectorChunkedSeq_next(PersistentVectorChunkedSeq *self) {
  if (Ptr_isReusable(self)) {
    PersistentVectorIterator it = self->it;
    PersistentVector_iteratorNext(&it);
    if (it.index >= it.parent->count) {
      Ptr_release(self);
      return RT_boxNil();
    }
    self->it = it;
    return RT_boxPtr(self);
  }

  PersistentVectorIterator it = self->it;
  PersistentVector_iteratorNext(&it);
  if (it.index >= it.parent->count) {
    Ptr_release(self);
    return RT_boxNil();
  }
  PersistentVectorChunkedSeq *res = PersistentVectorChunkedSeq_create(it);
  Ptr_release(self);
  return RT_boxPtr(res);
}

RTValue PersistentVectorChunkedSeq_more(PersistentVectorChunkedSeq *self) {
  RTValue n = PersistentVectorChunkedSeq_next(self); // consumes self
  if (RT_isNil(n)) {
    return RT_boxPtr(PersistentList_empty());
  }
  return n;
}

int32_t PersistentVectorChunkedSeq_count(PersistentVectorChunkedSeq *self) {
  int32_t res = (int32_t)(self->it.parent->count - self->it.index);
  Ptr_release(self);
  return res;
}

RTValue PersistentVectorChunkedSeq_chunkedFirst(PersistentVectorChunkedSeq *self) {
  ArrayChunk *chunk = ArrayChunk_create(self->it.block, self->it.blockIndex);
  Ptr_release(self);
  return RT_boxPtr(chunk);
}

RTValue PersistentVectorChunkedSeq_chunkedNext(PersistentVectorChunkedSeq *self) {
  if (Ptr_isReusable(self)) {
    // Skip current block
    uword_t remainingInBlock = self->it.block->count - self->it.blockIndex;
    self->it.index += remainingInBlock;
    if (self->it.index >= self->it.parent->count) {
      Ptr_release(self);
      return RT_boxNil();
    }
    // Move to next block
    self->it.block = PersistentVector_nthBlock(self->it.parent, self->it.index);
    self->it.blockIndex = 0;
    return RT_boxPtr(self);
  }

  // Skip current block
  PersistentVectorIterator it = self->it;
  uword_t remainingInBlock = it.block->count - it.blockIndex;
  it.index += remainingInBlock;
  if (it.index >= it.parent->count) {
    Ptr_release(self);
    return RT_boxNil();
  }
  // Move to next block
  it.block = PersistentVector_nthBlock(it.parent, it.index);
  it.blockIndex = 0;

  PersistentVectorChunkedSeq *res = PersistentVectorChunkedSeq_create(it);
  Ptr_release(self);
  return RT_boxPtr(res);
}

RTValue PersistentVectorChunkedSeq_chunkedMore(PersistentVectorChunkedSeq *self) {
  RTValue next = PersistentVectorChunkedSeq_chunkedNext(self);
  if (RT_isNil(next)) {
    return RT_boxPtr(PersistentList_empty());
  }
  return next;
}

RTValue PersistentVectorChunkedSeq_reduce(PersistentVectorChunkedSeq *self, RTValue f, RTValue start) {
  RTValue acc = start;
  FunctionMethod *method = Function_extractMethod(f, 2);

  size_t frameSize = sizeof(Frame) + 2 * sizeof(RTValue);
  Frame *frame = (Frame *)alloca(frameSize);
  frame->leafFrame = NULL;
  frame->bailoutEntryIndex = -1;

  RTValue args[2];
  PersistentVector *v = self->it.parent;
  uword_t startIdx = self->it.index;
  uword_t totalCount = v->count;
  uword_t tailCount = v->tail ? v->tail->count : 0;
  uword_t tailStart = totalCount - tailCount;

  // 1. Process Trie-resident blocks
  for (uword_t i = startIdx; i < tailStart;) {
    PersistentVectorNode *node = PersistentVector_nthBlock(v, i);
    uword_t blockEnd = (i & ~RRB_MASK) + 32;
    uword_t limit = blockEnd < tailStart ? blockEnd : tailStart;

    for (; i < limit; i++) {
      args[0] = acc;
      args[1] = node->array[i & RRB_MASK];
      retain(args[1]);
      acc = RT_invokeMethodWithFrame(frame, f, method, args, 2);
    }
  }

  // 2. Process the Tail
  if (startIdx < totalCount) {
    uword_t tailIdx = startIdx < tailStart ? 0 : startIdx - tailStart;
    PersistentVectorNode *tail = v->tail;
    for (uword_t j = tailIdx; j < tail->count; j++) {
      args[0] = acc;
      args[1] = tail->array[j];
      retain(args[1]);
      acc = RT_invokeMethodWithFrame(frame, f, method, args, 2);
    }
  }

  Ptr_release(self);
  release(f);
  return acc;
}

RTValue PersistentVectorChunkedSeq_reduce2(PersistentVectorChunkedSeq *self, RTValue f) {
  uword_t totalCount = self->it.parent->count;
  if (self->it.index >= totalCount) {
    Ptr_release(self);
    return RT_invokeDynamic(f, NULL, 0);
  }

  RTValue first = PersistentVector_iteratorGet(&self->it);
  retain(first);
  PersistentVector_iteratorNext(&self->it);

  if (self->it.index >= totalCount) {
    Ptr_release(self);
    release(f);
    return first;
  }

  return PersistentVectorChunkedSeq_reduce(self, f, first);
}

RTValue PersistentVectorChunkedSeq_cons(PersistentVectorChunkedSeq *self, RTValue item) {
  Ptr_release(self);
  release(item);
  throwIllegalArgumentException_C("cons not yet implemented for PersistentVectorChunkedSeq");
}

RTValue PersistentVectorChunkedSeq_empty(PersistentVectorChunkedSeq *self) {
  Ptr_release(self);
  return RT_boxPtr(PersistentList_empty());
}

RTValue PersistentVectorChunkedSeq_identity(PersistentVectorChunkedSeq *self) {
  return RT_boxPtr(self);
}

RTValue PersistentVectorChunkedSeq_drop(PersistentVectorChunkedSeq *self, int32_t n) {
  if (n <= 0)
    return RT_boxPtr(self);

  if (Ptr_isReusable(self)) {
    self->it.index += n;
    if (self->it.index >= self->it.parent->count) {
      Ptr_release(self);
      return RT_boxNil();
    }
    // Refresh block
    self->it.block = PersistentVector_nthBlock(self->it.parent, self->it.index);
    self->it.blockIndex = self->it.index & RRB_MASK;
    return RT_boxPtr(self);
  }

  PersistentVectorIterator it = self->it;
  it.index += n;
  if (it.index >= it.parent->count) {
    Ptr_release(self);
    return RT_boxNil();
  }

  // Refresh block
  it.block = PersistentVector_nthBlock(it.parent, it.index);
  it.blockIndex = it.index & RRB_MASK;

  PersistentVectorChunkedSeq *res = PersistentVectorChunkedSeq_create(it);
  Ptr_release(self);
  return RT_boxPtr(res);
}
