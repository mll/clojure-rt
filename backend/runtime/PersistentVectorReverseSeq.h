#ifndef RT_PERSISTENT_VECTOR_REVERSE_SEQ_H
#define RT_PERSISTENT_VECTOR_REVERSE_SEQ_H

#include "RTValue.h"
#include "PersistentVector.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Object super;
    PersistentVector *v;
    int32_t index;
} PersistentVectorReverseSeq;

RTValue PersistentVectorReverseSeq_create(PersistentVector *v, int32_t index);
void PersistentVectorReverseSeq_destroy(PersistentVectorReverseSeq *self, bool deallocateChildren);
RTValue PersistentVectorReverseSeq_first(PersistentVectorReverseSeq *self);
RTValue PersistentVectorReverseSeq_next(PersistentVectorReverseSeq *self);
RTValue PersistentVectorReverseSeq_more(PersistentVectorReverseSeq *self);
int32_t PersistentVectorReverseSeq_count(PersistentVectorReverseSeq *self);
RTValue PersistentVectorReverseSeq_empty(PersistentVectorReverseSeq *self);
RTValue PersistentVectorReverseSeq_cons(PersistentVectorReverseSeq *self, RTValue item);
String *PersistentVectorReverseSeq_toString(PersistentVectorReverseSeq *self);
bool PersistentVectorReverseSeq_equals_managed(PersistentVectorReverseSeq *self, RTValue other);

#ifdef __cplusplus
}
#endif

#endif
