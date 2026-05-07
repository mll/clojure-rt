#ifndef RT_PERSISTENT_VECTOR_CHUNKED_SEQ_H
#define RT_PERSISTENT_VECTOR_CHUNKED_SEQ_H

struct ExecutionContext;

#include "ObjectProto.h"
#include "PersistentVectorIterator.h"
#include "RTValue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct String String;

struct PersistentVectorChunkedSeq {
  Object super;
  PersistentVectorIterator it;
};

typedef struct PersistentVectorChunkedSeq PersistentVectorChunkedSeq;

PersistentVectorChunkedSeq *PersistentVectorChunkedSeq_create(PersistentVectorIterator it);
void PersistentVectorChunkedSeq_destroy(PersistentVectorChunkedSeq *self, bool deallocateChildren);
uint64_t PersistentVectorChunkedSeq_hash(PersistentVectorChunkedSeq *self);
bool PersistentVectorChunkedSeq_equals(PersistentVectorChunkedSeq *self, PersistentVectorChunkedSeq *other);
bool PersistentVectorChunkedSeq_equals_managed(PersistentVectorChunkedSeq *self, RTValue other);
String *PersistentVectorChunkedSeq_toString(PersistentVectorChunkedSeq *self);

// Clojure Interface
RTValue PersistentVectorChunkedSeq_first(PersistentVectorChunkedSeq *self);
RTValue PersistentVectorChunkedSeq_next(PersistentVectorChunkedSeq *self);
RTValue PersistentVectorChunkedSeq_more(PersistentVectorChunkedSeq *self);
int32_t PersistentVectorChunkedSeq_count(PersistentVectorChunkedSeq *self);

// IPersistentVectorChunkedSeq
RTValue PersistentVectorChunkedSeq_chunkedFirst(PersistentVectorChunkedSeq *self);
RTValue PersistentVectorChunkedSeq_chunkedNext(PersistentVectorChunkedSeq *self);
RTValue PersistentVectorChunkedSeq_chunkedMore(PersistentVectorChunkedSeq *self);

// IReduce
RTValue PersistentVectorChunkedSeq_reduce(__attribute__((swift_context)) struct ExecutionContext *ctx, PersistentVectorChunkedSeq *self, RTValue f, RTValue start) __attribute__((swiftcall));
RTValue PersistentVectorChunkedSeq_reduce2(__attribute__((swift_context)) struct ExecutionContext *ctx, PersistentVectorChunkedSeq *self, RTValue f) __attribute__((swiftcall));
RTValue PersistentVectorChunkedSeq_cons(PersistentVectorChunkedSeq *self, RTValue item);
RTValue PersistentVectorChunkedSeq_empty(PersistentVectorChunkedSeq *self);
RTValue PersistentVectorChunkedSeq_identity(PersistentVectorChunkedSeq *self);

// IDrop
RTValue PersistentVectorChunkedSeq_drop(PersistentVectorChunkedSeq *self, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
