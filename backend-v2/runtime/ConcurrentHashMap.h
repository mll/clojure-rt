#ifndef RT_CONCURRENT_HASH_MAP
#define RT_CONCURRENT_HASH_MAP
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
#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"
#include "defines.h"

/* https://preshing.com/20160201/new-concurrent-hash-maps-for-cpp/
   Leapfrog */

typedef struct Object Object;

#define CHM_REDIRECT 1
#define CHM_EMPTY 0

typedef struct ConcurrentHashMapEntry {
  _Atomic(RTValue) key;
  _Atomic(RTValue) value;
  _Atomic(uword_t) keyHash;
  _Atomic(unsigned short) leaps;
} ConcurrentHashMapEntry;

typedef struct ConcurrentHashMapNode {
  uword_t sizeMask;
  short int resizingThreshold;
  ConcurrentHashMapEntry array[];
} ConcurrentHashMapNode;

typedef struct ConcurrentHashMap {
  Object super;
  _Atomic(ConcurrentHashMapNode *) root;
} ConcurrentHashMap;

ConcurrentHashMap *ConcurrentHashMap_create(unsigned char initialSizeExponent);

void ConcurrentHashMap_assoc(ConcurrentHashMap *self, RTValue key,
                             RTValue value);
void ConcurrentHashMap_dissoc(ConcurrentHashMap *self, RTValue key);
RTValue ConcurrentHashMap_get(ConcurrentHashMap *self, RTValue key);

void ConcurrentHashMap_assoc_preservesSelf(ConcurrentHashMap *self, RTValue key,
                                           RTValue value,
                                           bool releaseSelfOnError);
void ConcurrentHashMap_dissoc_preservesSelf(ConcurrentHashMap *self,
                                            RTValue key);
RTValue ConcurrentHashMap_get_preservesSelf(ConcurrentHashMap *self,
                                            RTValue key);
String *ConcurrentHashMap_toString(ConcurrentHashMap *self);

bool ConcurrentHashMap_equals(ConcurrentHashMap *self,
                              ConcurrentHashMap *other);
uword_t ConcurrentHashMap_hash(ConcurrentHashMap *self);
void ConcurrentHashMap_destroy(ConcurrentHashMap *self);

#ifdef __cplusplus
}
#endif

#endif
