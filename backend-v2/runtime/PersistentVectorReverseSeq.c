#include "PersistentVectorReverseSeq.h"
#include "Exceptions.h"
#include "Object.h"
#include "PersistentList.h"

RTValue PersistentVectorReverseSeq_create(PersistentVector *v, int32_t index) {
  if (index < 0)
    return RT_boxNil();
  PersistentVectorReverseSeq *self = (PersistentVectorReverseSeq *)allocate(
      sizeof(PersistentVectorReverseSeq));
  Object_create((Object *)self, persistentVectorReverseSeqType);
  self->v = v;
  Ptr_retain(v);
  self->index = index;
  return RT_boxPtr(self);
}

void PersistentVectorReverseSeq_destroy(PersistentVectorReverseSeq *self,
                                        bool deallocateChildren) {
  Ptr_release(self->v);
}

RTValue PersistentVectorReverseSeq_first(PersistentVectorReverseSeq *self) {
  Ptr_retain(self->v);
  RTValue val = PersistentVector_nth(self->v, self->index);
  Ptr_release(self);
  return val;
}

RTValue PersistentVectorReverseSeq_next(PersistentVectorReverseSeq *self) {
  if (self->index <= 0) {
    Ptr_release(self);
    return RT_boxNil();
  }
  if (Ptr_isReusable(self)) {
    self->index--;
    return RT_boxPtr(self);
  }
  RTValue res = PersistentVectorReverseSeq_create(self->v, self->index - 1);
  Ptr_release(self);
  return res;
}

RTValue PersistentVectorReverseSeq_more(PersistentVectorReverseSeq *self) {
  RTValue n = PersistentVectorReverseSeq_next(self);
  if (RT_isNil(n)) {
    return RT_boxPtr(PersistentList_empty());
  }
  return n;
}

int32_t PersistentVectorReverseSeq_count(PersistentVectorReverseSeq *self) {
  int32_t res = self->index + 1;
  Ptr_release(self);
  return res;
}

RTValue PersistentVectorReverseSeq_empty(PersistentVectorReverseSeq *self) {
  Ptr_release(self);
  return RT_boxPtr(PersistentList_empty());
}

String *PersistentVectorReverseSeq_toString(PersistentVectorReverseSeq *self) {
  String *res = String_createStatic("(");
  bool first = true;
  for (int32_t i = self->index; i >= 0; i--) {
    if (!first) {
      res = String_concat(res, String_createStatic(" "));
    }
    RTValue val = PersistentVector_nth(self->v, i);
    retain(val);
    res = String_concat(res, toString(val));
    first = false;
  }
  res = String_concat(res, String_createStatic(")"));
  Ptr_release(self);
  return res;
}

RTValue PersistentVectorReverseSeq_cons(PersistentVectorReverseSeq *self,
                                        RTValue item) {
  Ptr_release(self);
  release(item);
  throwIllegalArgumentException_C(
      "cons not yet implemented for PersistentVectorReverseSeq");
}

bool PersistentVectorReverseSeq_equals_managed(PersistentVectorReverseSeq *self,
                                               RTValue other) {
  // Simple implementation for now
  Ptr_release(self);
  release(other);
  return false;
}
