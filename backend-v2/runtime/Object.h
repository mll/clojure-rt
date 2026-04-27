#ifndef RT_OBJECT
#define RT_OBJECT

#include "Hash.h"
#include "word.h"
#ifdef __cplusplus
extern "C" {
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"

#include "Ebr.h"
#include "PersistentVectorIterator.h"
#include "RTValue.h"
#include "defines.h"
#include <assert.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
// Bit 0 is the shared flag.
// Remaining bits are the count.
#define SHARED_BIT ((uword_t)1)
#define COUNT_INC ((uword_t)2)

typedef struct String String;

#define MEMORY_BANK_SIZE_MAX 10

extern void logBacktrace();
void printReferenceCounts();

extern _Atomic(uword_t) allocationCount[256];
extern _Atomic(uword_t) objectCount[256];
extern _Atomic(uword_t) globalMethodICEpoch;
#define TRACING_LIMIT 256

// bank 0 - 32 bytes 2^5
// bank 1 - 64 bytes
// bank 2 - 128 bytes
// bank 3 - 256 bytes
// bank 4 - 512
// bank 5 - 1024
// bank 6 - 2048
// bank 7 - 4096

#if defined(COMPILING_RUNTIME_BITCODE) && !defined(__APPLE__)
#include "TLS_Support.h"
extern uintptr_t memoryBank_offset;
extern uintptr_t memoryBankSize_offset;
#define memoryBank                                                             \
  (*(void ***)((char *)JITEngine_getThreadPointer() + memoryBank_offset))
#define memoryBankSize                                                         \
  (*(int **)((char *)JITEngine_getThreadPointer() + memoryBankSize_offset))
#else
extern _Thread_local void *memoryBank[8];
extern _Thread_local int memoryBankSize[8];
#endif

void initialise_memory();
inline void retain(RTValue self);
inline bool release(RTValue self);
inline void promoteToShared(RTValue self);
inline uword_t hash(RTValue v);
inline bool equals(RTValue v1, RTValue v2);
inline bool equals_managed(RTValue v1, RTValue v2);
inline objectType getType(RTValue v);
inline String *toString(RTValue v);
inline void Object_retain(Object *self);
inline bool Object_release(Object *self);
inline void Object_promoteToShared(Object *self);
inline void Object_create(Object *self, objectType type);
inline uword_t Object_hash(Object *self);
inline bool Object_equals(Object *self, Object *other);
inline bool Object_isReusable(Object *self);
inline bool isReusable(RTValue v);
inline void Object_promoteToSharedShallow(Object *self, uword_t current);
inline uword_t Object_getRawRefCount(Object *self);
inline bool Object_release_internal(Object *self, bool deallocateChildren);
inline void Ptr_retain(void *ptr);
inline bool Ptr_release(void *ptr);
inline void Ptr_autorelease(void *ptr);
inline uword_t Ptr_hash(void *ptr);
inline bool Ptr_equals(void *ptr, void *other);
inline bool Ptr_isReusable(void *ptr);

#include "BigInteger.h"
#include "Boolean.h"
#include "BridgedObject.h"
#include "Class.h"
#include "ConcurrentHashMap.h"
#include "Double.h"
#include "Exception.h"
#include "Function.h"
#include "Integer.h"
#include "Keyword.h"
#include "Nil.h"
#include "PersistentArrayMap.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorIterator.h"
#include "PersistentVectorNode.h"
#include "Ratio.h"
#include "String.h"
#include "StringBuilder.h"
#include "Symbol.h"
#include "Var.h"

inline void *allocate(size_t size) {
#ifndef USE_MEMORY_BANKS
  return malloc(size);
#else
  int bankId;
  if (size <= 24)
    bankId = 0;
  else if (size <= 64)
    bankId = 1;
  else if (size <= 128)
    bankId = 2;
  else if (size <= 256)
    bankId = 3;
  else if (size <= 296)
    bankId = 4;
  else if (size <= 1024)
    bankId = 5;
  else if (size <= 2048)
    bankId = 6;
  else if (size <= 4096)
    bankId = 7;
  else {
    // Not from a bank:
    printf("Not from bank! : %zu\n", size);
    Object *hdr = (Object *)malloc(size);
    if (!hdr)
      return NULL;
    hdr->bankId = 0xFF; // 0xFF marks a non-bank allocation
    return hdr;
  }

  void *candidate = memoryBank[bankId];
  if (candidate) {
    //      printf("Found in bank %d : %zu - %p\n", bankId, size, candidate);
    memoryBank[bankId] = *((void **)candidate);
    memoryBankSize[bankId]--;
    // candidate already points to user space which is after the header
    return candidate;
  }

  // No free block in bank, allocate a new one
  size_t blockSize;
  if (bankId == 4)
    blockSize = 296;
  else if (bankId == 0)
    blockSize = 24;
  else {
    blockSize = 1 << (5 + bankId);
  }
  Object *hdr = (Object *)malloc(blockSize);
  if (!hdr)
    return NULL;
  hdr->bankId = (unsigned char)bankId;
  return hdr;
#endif
}

inline void deallocate(void *ptr) {
#ifndef USE_MEMORY_BANKS
  free(ptr);
  return;
#else
  if (!ptr)
    return;
  Object *hdr = (Object *)ptr;
  unsigned char bankId = hdr->bankId;

  // If it's not from a bank, free directly
  if (bankId == 0xFF || memoryBankSize[bankId] >= MEMORY_BANK_SIZE_MAX) {
    free(hdr);
    return;
  }
  //    printf("Pushing to memory bank %d - %p\n", bankId, ptr);
  // Push back into the memory bank free list
  *((void **)ptr) = memoryBank[bankId];
  memoryBank[bankId] = ptr;
  memoryBankSize[bankId]++;
#endif
}

/* Outside of refcount system */
inline objectType getType(RTValue v) {
  // 1. Fast Path: Tagged Types (Int, Bool, Nil, etc.) - Priority Optimized
  if (__builtin_expect(v >= RT_TAG_DOUBLE_START, 1)) {
    uint16_t tag = (uint16_t)(v >> 48);
    // Optimize for non-pointer inline types
    if (__builtin_expect(tag != (uint16_t)(RT_TAG_PTR >> 48), 1))
      return (objectType)(tag & 0xF);
    // Fallback for pointers
    return ((Object *)RT_unboxPtr(v))->type;
  }
  // 2. Second Path: Doubles
  return doubleType;
}

inline void Object_retain(Object *restrict self) {
//  printf("RETAIN!!! %d\n", self->type);
#ifdef REFCOUNT_TRACING
  if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
    atomic_fetch_add_explicit(&(allocationCount[self->type - 1]), 1,
                              memory_order_relaxed);
  }
#endif
#ifdef REFCOUNT_NONATOMIC
  self->refCount += COUNT_INC;
#else
  uword_t current =
      atomic_load_explicit(&(self->atomicRefCount), memory_order_relaxed);

  if (!(current & SHARED_BIT)) {
    // FAST PATH: It's local. Use a plain non-atomic write.
    // We can do this because only THIS thread owns it.
    uword_t new_val = current + COUNT_INC;
    atomic_store_explicit(&(self->atomicRefCount), new_val,
                          memory_order_relaxed);
  } else {
    // SLOW PATH: It's shared. Must use atomic fetch_add.
    atomic_fetch_add_explicit(&(self->atomicRefCount), COUNT_INC,
                              memory_order_relaxed);
  }
#endif
}

/* TODO: an improvement could be that for collections if we detect that it
  should be destroyed, we do retain; autorelease; and ignore destroy. This would
  free the current thread from the burden of deallocating them.

  Difficulties:
           - how does it interact with shared bit in refcount?
           - how to avoid endless loop (the cleanup thread needs to do a real
             destroy)
 */
inline void Object_destroy(Object *restrict self, bool deallocateChildren) {
  //  printf("--> Deallocating type %d addres %lld\n", self->type, (uword_t));
  // printReferenceCounts();
  switch ((objectType)self->type) {
  case stringType:
    String_destroy((String *)self);
    break;
  case classType:
    Class_destroy((Class *)self);
    break;
  case persistentListType:
    PersistentList_destroy((PersistentList *)self, deallocateChildren);
    break;
  case persistentVectorType:
    PersistentVector_destroy((PersistentVector *)self, deallocateChildren);
    break;
  case persistentVectorNodeType:
    PersistentVectorNode_destroy((PersistentVectorNode *)self,
                                 deallocateChildren);
    break;
  case concurrentHashMapType:
    ConcurrentHashMap_destroy((ConcurrentHashMap *)self);
    break;
  case functionType:
    Function_destroy((ClojureFunction *)self);
    break;
  case bigIntegerType:
    BigInteger_destroy((BigInteger *)self);
    break;
  case ratioType:
    Ratio_destroy((Ratio *)self);
    break;
  case persistentArrayMapType:
    PersistentArrayMap_destroy((PersistentArrayMap *)self, deallocateChildren);
    break;
  case varType:
    Var_destroy((Var *)self);
    break;
  case exceptionType:
    Exception_destroy((Exception *)self);
    break;
  case bridgedObjectType:
    BridgedObject_destroy((BridgedObject *)self);
    break;
  case stringBuilderType:
    StringBuilder_destroy((StringBuilder *)self);
    break;

  default:
    break;
  }
  deallocate(self);
  // printf("dealloc end %lld\n", (uword_t));
  // printReferenceCounts();
  // printf("=========================\n");
}

inline bool Object_isReusable(Object *restrict self) {
  uword_t current =
      atomic_load_explicit(&(self->atomicRefCount), memory_order_acquire);
  if (current >> 1 == 1) {
    if (current & SHARED_BIT) {
      // It was shared, but now we are the exclusive owner.
      // De-promote to local for performance and correct propagation of shared
      // bit in the future.
      atomic_store_explicit(&(self->atomicRefCount), COUNT_INC,
                            memory_order_relaxed);
    }
    return true;
  }
  return false;
}

inline bool isReusable(RTValue self) {
  return RT_isPtr(self) && Object_isReusable((Object *)RT_unboxPtr(self));
}

inline bool Object_release_internal(Object *restrict self,
                                    bool deallocateChildren) {
#ifdef REFCOUNT_TRACING
  if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
    atomic_fetch_sub_explicit(&(allocationCount[self->type - 1]), 1,
                              memory_order_relaxed);
  }
#endif
#ifdef REFCOUNT_NONATOMIC
  self->refCount -= COUNT_INC;
  if ((self->refCount >> 1) == 0) {
#ifdef REFCOUNT_TRACING
    if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
      uword_t countVal = atomic_fetch_sub_explicit(
          &(objectCount[self->type - 1]), 1, memory_order_relaxed);
      assert(countVal >= 1 && "Memory corruption!");
    }
#endif
    Object_destroy(self, deallocateChildren);
    return true;
  }
#else
  uintptr_t current =
      atomic_load_explicit(&(self->atomicRefCount), memory_order_relaxed);
  if (!(current & SHARED_BIT)) {
    // --- FAST PATH: Local ---
    // Since it's local, only THIS thread can be releasing it.
    // No other thread can be concurrent here.
    uword_t newVal = current - COUNT_INC;

    if ((newVal >> 1) == 0) {
#ifdef REFCOUNT_TRACING
      if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
        uword_t countVal = atomic_fetch_sub_explicit(
            &(objectCount[self->type - 1]), 1, memory_order_relaxed);
        assert(countVal >= 1 && "Memory corruption!");
      }
#endif
      Object_destroy(self, deallocateChildren);
      return true;
    }
    atomic_store_explicit(&(self->atomicRefCount), newVal,
                          memory_order_relaxed);
  } else {
    // --- SLOW PATH: Shared ---
    // We must use atomic operations because other threads might be accessing
    // this object.
    uword_t relVal = atomic_fetch_sub_explicit(&(self->atomicRefCount),
                                               COUNT_INC, memory_order_release);
    if (relVal >> 1 == 1) {
#ifdef REFCOUNT_TRACING
      if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
        uword_t countVal = atomic_fetch_sub_explicit(
            &(objectCount[self->type - 1]), 1, memory_order_relaxed);
        assert(countVal >= 1 && "Memory corruption!");
      }
#endif
      atomic_thread_fence(memory_order_acquire);
      Object_destroy(self, deallocateChildren);
      return true;
    }
  }
#endif
  return false;
}

/* outside of refcount system */
inline void Object_promoteToSharedShallow(Object *restrict self,
                                          uword_t current) {
#ifndef REFCOUNT_NONATOMIC
  // Optimization: If we guarantee that only the 'owning' thread performs
  // the initial promotion, we can use relaxed load/store.
  // This is significantly faster than atomic RMW (fetch_or) in bulk operations.
  if (!(current & SHARED_BIT)) {
    atomic_store_explicit(&(self->atomicRefCount), current | SHARED_BIT,
                          memory_order_relaxed);
  }
#else
  self->refCount |= SHARED_BIT;
#endif
}

/* outside of refcount system */
inline uword_t Object_getRawRefCount(Object *self) {
  return atomic_load_explicit(&self->atomicRefCount, memory_order_relaxed);
}

/* outside of refcount system */
inline void Object_promoteToShared(Object *restrict self) {
  uword_t count = Object_getRawRefCount(self);
  if ((count & SHARED_BIT) != 0) {
    return;
  }

  switch ((objectType)self->type) {
  case persistentListType:
    PersistentList_promoteToShared((PersistentList *)self, count);
    break;

  case persistentArrayMapType:
    PersistentArrayMap_promoteToShared((PersistentArrayMap *)self, count);
    break;

  case persistentVectorType:
    PersistentVector_promoteToShared((PersistentVector *)self, count);
    break;

  case persistentVectorNodeType:
    PersistentVectorNode_promoteToShared((PersistentVectorNode *)self, count);
    break;

  case functionType:
    Function_promoteToShared((ClojureFunction *)self, count);
    break;

  default:
    Object_promoteToSharedShallow(self, count);
    break;
  }
}

/* Outside of refcount system */
inline uword_t Object_hash(Object *restrict self) {
  switch ((objectType)self->type) {
  case stringType:
    return String_hash((String *)self);
    break;
  case classType:
    return Class_hash((Class *)self);
    break;
  case persistentListType:
    return PersistentList_hash((PersistentList *)self);
    break;
  case persistentVectorType:
    return PersistentVector_hash((PersistentVector *)self);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_hash((PersistentVectorNode *)self);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_hash((ConcurrentHashMap *)self);
  case functionType:
    return Function_hash((ClojureFunction *)self);
  case bigIntegerType:
    return BigInteger_hash((BigInteger *)self);
  case ratioType:
    return Ratio_hash((Ratio *)self);
  case persistentArrayMapType:
    return PersistentArrayMap_hash((PersistentArrayMap *)self);
  case varType:
    return Var_hash((Var *)self);
  case exceptionType:
    return Exception_hash((Exception *)self);
  case bridgedObjectType:
    return BridgedObject_hash((BridgedObject *)self);
  case stringBuilderType:
    return StringBuilder_hash((StringBuilder *)self);
  default:
    assert(false && "Internal error: hash computation for NaN tagged types "
                    "should be computed earlier.");
  }
  // To silence the compiler warning, this code should never be reached.
  return 0;
}

/* Outside of refcount system */
inline uword_t hash(RTValue v) {
  objectType t = getType(v);
  switch (t) {
  case integerType:
    return (uword_t)avalanche(RT_unboxInt32(v));
  case booleanType:
    return (uword_t)avalanche(RT_unboxBool(v));
  case nilType:
    return 0xDEADBEEF; // Standard nil hash
  case keywordType:
    return (uword_t)avalanche(RT_unboxKeyword(v));
  case symbolType:
    return (uword_t)avalanche(RT_unboxSymbol(v));
  case doubleType: {
    double d = RT_unboxDouble(v);
    uint64_t u;
    memcpy(&u, &d, 8);
    return avalanche(u);
  }
  default:
    assert(RT_isPtr(v) &&
           "Internal error - trying to compute hash from an unknown value");
    return Object_hash((Object *)RT_unboxPtr(v));
  }
}

/* Outside of refcount system */
inline bool Object_equals(Object *self, Object *other) {
  if (Object_hash(self) != Object_hash(other))
    return false;

  switch ((objectType)self->type) {
  case stringType:
    return String_equals((String *)self, (String *)other);
    break;
  case classType:
    return Class_equals((Class *)self, (Class *)other);
    break;
  case persistentListType:
    return PersistentList_equals((PersistentList *)self,
                                 (PersistentList *)other);
    break;
  case persistentVectorType:
    return PersistentVector_equals((PersistentVector *)self,
                                   (PersistentVector *)other);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_equals((PersistentVectorNode *)self,
                                       (PersistentVectorNode *)other);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_equals((ConcurrentHashMap *)self,
                                    (ConcurrentHashMap *)other);
    break;
  case functionType:
    return Function_equals((ClojureFunction *)self, (ClojureFunction *)other);
    break;
  case bigIntegerType:
    return BigInteger_equals((BigInteger *)self, (BigInteger *)other);
    break;
  case ratioType:
    return Ratio_equals((Ratio *)self, (Ratio *)other);
    break;
  case persistentArrayMapType:
    return PersistentArrayMap_equals((PersistentArrayMap *)self,
                                     (PersistentArrayMap *)other);
    break;
  case varType:
    return Var_equals((Var *)self, (Var *)other);
    break;
  case exceptionType:
    return Exception_equals((Exception *)self, (Exception *)other);
  case bridgedObjectType:
    return BridgedObject_equals((BridgedObject *)self, (BridgedObject *)other);
  case stringBuilderType:
    return StringBuilder_equals((StringBuilder *)self, (StringBuilder *)other);
  default:
    assert(false && "Internal error: hash computation for NaN tagged types "
                    "should be computed earlier.");
  }
  // To silence the compiler warning, this code should never be reached.
  return false;
}

/* Outside of refcount system - should it be like this? It probably shouldnt
 */
inline bool equals(RTValue self, RTValue other) {
  if (self == other)
    return true;

  bool leftPtr = RT_isPtr(self);
  bool rightPtr = RT_isPtr(other);
  if (rightPtr != leftPtr || !leftPtr)
    return false;

  objectType t1 = getType(self);
  objectType t2 = getType(other);
  if (t1 != t2)
    return false;

  return Object_equals((Object *)RT_unboxPtr(self),
                       (Object *)RT_unboxPtr(other));
}

inline String *Object_toString(Object *restrict self) {
  switch ((objectType)self->type) {
  case stringType:
    return String_toString((String *)self);
    break;
  case classType:
    return Class_toString((Class *)self);
    break;
  case persistentListType:
    return PersistentList_toString((PersistentList *)self);
    break;
  case persistentVectorType:
    return PersistentVector_toString((PersistentVector *)self);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_toString((PersistentVectorNode *)self);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_toString((ConcurrentHashMap *)self);
  case functionType:
    return Function_toString((ClojureFunction *)self);
  case bigIntegerType:
    return BigInteger_toString((BigInteger *)self);
  case ratioType:
    return Ratio_toString((Ratio *)self);
  case persistentArrayMapType:
    return PersistentArrayMap_toString((PersistentArrayMap *)self);
  case varType:
    return Var_toString((Var *)self);
  case exceptionType:
    return Exception_toString((Exception *)self);
  case bridgedObjectType:
    return BridgedObject_toString((BridgedObject *)self);
  case stringBuilderType:
    return StringBuilder_toString((StringBuilder *)self);
  default:
    assert(false && "Internal error: Object_toString got an unsupported type");
  }
  // To silence the compiler warning, this code should never be reached.
  return NULL;
}

inline bool Object_release(Object *restrict self) {
  return Object_release_internal(self, true);
}

inline void retain(RTValue self) {
  if (RT_isPtr(self)) {
    Object_retain((Object *)RT_unboxPtr(self));
  }
}

inline bool release(RTValue self) {
  if (RT_isPtr(self)) {
    return Object_release((Object *)RT_unboxPtr(self));
  }
  return false;
}

inline void promoteToShared(RTValue self) {
  if (RT_isPtr(self)) {
    Object_promoteToShared((Object *)RT_unboxPtr(self));
  }
}

inline void Object_create(Object *restrict self, objectType type) {
#ifdef REFCOUNT_NONATOMIC
  self->refCount = COUNT_INC;
#else
  atomic_store_explicit(&(self->atomicRefCount), COUNT_INC,
                        memory_order_relaxed);
#endif
  self->type = type;
#ifdef REFCOUNT_TRACING
  if (self->type > 0 && (int)self->type <= TRACING_LIMIT) {
    atomic_fetch_add_explicit(&(allocationCount[self->type - 1]), 1,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&(objectCount[self->type - 1]), 1,
                              memory_order_relaxed);
  }
#endif
  //  printf("--> Allocating type %d addres %p\n", self->type, );
}

inline uword_t combineHash(uword_t lhs, uword_t rhs) {
  lhs ^= rhs + 0x9ddfea08eb382d69ULL + (lhs << 6) + (lhs >> 2);
  return lhs;
}

inline void Ptr_autorelease(void *self) { autorelease(RT_boxPtr(self)); }
inline void Ptr_retain(void *self) { Object_retain((Object *)self); }
inline bool Ptr_release(void *self) { return Object_release((Object *)self); }
inline uword_t Ptr_hash(void *self) { return Object_hash((Object *)self); }
inline bool Ptr_isReusable(void *self) {
  return Object_isReusable((Object *)self);
}
inline bool Ptr_equals(void *self, void *other) {
  if (self == other)
    return true;
  return Object_equals((Object *)self, (Object *)other);
}

inline String *toString(RTValue self) {
  if (RT_isInt32(self))
    return Integer_toString(self);
  if (RT_isDouble(self))
    return Double_toString(self);
  if (RT_isBool(self))
    return Boolean_toString(self);
  if (RT_isNil(self)) {
    release(self);
    return Nil_toString();
  }
  if (RT_isKeyword(self))
    return Keyword_toString(self);
  if (RT_isSymbol(self))
    return Symbol_toString(self);

  assert(RT_isPtr(self) && "Internal error: Not a pointer");
  return Object_toString((Object *)RT_unboxPtr(self));
}

inline bool equals_managed(RTValue self, RTValue other) {
  bool result = equals(self, other);
  release(self);
  release(other);
  return result;
}

inline bool identical_managed(RTValue v1, RTValue v2) {
  bool result = (v1 == v2);
  release(v1);
  release(v2);
  return result;
}


#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop

#endif