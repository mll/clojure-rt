#ifndef RT_TRANSIENT
#define RT_TRANSIENT

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

#define PERSISTENT 0

uint64_t getTransientID();
void assert_transient(uint64_t transientID);
void assert_persistent(uint64_t transientID);

#endif
