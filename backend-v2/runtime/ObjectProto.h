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
  integerType = 1,
  doubleType,               // 2
  nilType,                  // 3
  booleanType,              // 4
  symbolType,               // 5
  keywordType,              // 6
  stringType,               // 7
  persistentListType,       // 8
  persistentVectorType,     // 9
  persistentVectorNodeType, // 10
  concurrentHashMapType,    // 11
  functionType,             // 12
  bigIntegerType,           // 13
  ratioType,                // 14
  classType,                // 15
  persistentArrayMapType,   // 16
  varType,                  // 17
  objectRootType,           // 18
  exceptionType,            // 19
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
