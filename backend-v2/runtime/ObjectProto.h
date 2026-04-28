#ifndef OBJECT_PROTO_H
#define OBJECT_PROTO_H
#ifdef __cplusplus
#include <atomic>
using std::atomic_compare_exchange_strong_explicit;
using std::atomic_compare_exchange_weak_explicit;
using std::atomic_exchange_explicit;
using std::atomic_fetch_add_explicit;
using std::atomic_fetch_sub_explicit;
using std::atomic_load_explicit;
using std::atomic_store_explicit;
using std::memory_order;
using std::memory_order_acq_rel;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_seq_cst;
#define _Atomic(X) std::atomic<X>
#else
#include <stdatomic.h>
#endif
#include "word.h"

enum objectType {
  // Heap types
  stringType = 1,
  doubleType = 2,
  persistentListType = 3,
  persistentVectorType = 4,
  persistentVectorNodeType = 5,
  concurrentHashMapType = 6,
  functionType = 7,
  bigIntegerType = 8,

  // Tagged types (Mapped to 0xFFF[9-F] nibbles)
  nullType = 9,          // 0xFFF9
  symbolType = 10,       // 0xFFFA
  keywordType = 11,      // 0xFFFB
  nilType = 12,          // 0xFFFC
  booleanType = 13,      // 0xFFFD
  // 14 reserved for ptr
  integerType = 15,      // 0xFFFF

  // Remaining types
  ratioType = 16,
  classType = 17,
  persistentArrayMapType = 18,
  varType = 19,
  objectRootType = 20,
  exceptionType = 21,
  bridgedObjectType = 22,
  stringBuilderType = 23,
  persistentVectorChunkedSeqType = 24,
  arrayChunkType = 25,
  persistentVectorReverseSeqType = 26,
};

typedef enum objectType objectType;

struct Object {
#ifdef REFCOUNT_NONATOMIC
  uword_t refCount;
#endif
  _Atomic(uword_t) atomicRefCount;
  objectType type;
#ifdef USE_MEMORY_BANKS
  unsigned char bankId;
#endif
};
typedef struct Object Object;

#endif
