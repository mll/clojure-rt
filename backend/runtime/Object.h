#ifndef RT_OBJECT
#define RT_OBJECT

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include <stdatomic.h>
#include "String.h"
#include "Integer.h"
#include "Double.h"
#include "Boolean.h"
#include "Nil.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "ConcurrentHashMap.h"
#include "Symbol.h"
#include "Keyword.h"
#include "Class.h"
#include "Deftype.h"
#include <assert.h>
#include <execinfo.h>
#include "Function.h"
#include "Var.h"
#include "BigInteger.h"
#include "Ratio.h"
#include "PersistentArrayMap.h"
typedef struct String String; 

#define MEMORY_BANK_SIZE_MAX 10

extern void logBacktrace();
void printReferenceCounts();

struct Object {
  objectType type;
  unsigned char bankId;
#ifdef REFCOUNT_NONATOMIC
  uint64_t refCount;
#endif
  volatile atomic_uint_fast64_t atomicRefCount;
};

typedef struct Object Object; 

extern _Atomic uint64_t allocationCount[256]; 
extern _Atomic uint64_t objectCount[256]; 

// bank 0 - 32 bytes 2^5
// bank 1 - 64 bytes
// bank 2 - 128 bytes
// bank 3 - 256 bytes
// bank 4 - 512
// bank 5 - 1024
// bank 6 - 2048
// bank 7 - 4096

extern _Thread_local void *memoryBank[8];
extern _Thread_local int memoryBankSize[8];

void initialise_memory();


inline void *allocate(size_t size) {
#ifndef USE_MEMORY_BANKS
    return malloc(size);
#endif 
    int bankId;
    if (size <= 24) bankId = 0;
    else if (size <= 64) bankId = 1;
    else if (size <= 128) bankId = 2;
    else if (size <= 256) bankId = 3;
    else if (size <= 296) bankId = 4;
    else if (size <= 1024) bankId = 5;
    else if (size <= 2048) bankId = 6;
    else if (size <= 4096) bankId = 7;
    else {
        // Not from a bank:
        printf("Not from bank! : %zu\n", size);
        Object *hdr = (Object *)malloc(size);
        if (!hdr) return NULL;
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
    if(bankId == 4) blockSize = 296;
    else if(bankId == 0) blockSize = 24;
    else {
      blockSize = 1 << (5 + bankId);
    }
    Object *hdr = (Object *)malloc(blockSize);
    if (!hdr) return NULL;
    hdr->bankId = (unsigned char)bankId;
    return hdr;
}

inline void deallocate(void *ptr) {
#ifndef USE_MEMORY_BANKS
  free(ptr);
  return;
#endif
    if (!ptr) return;
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
}

inline void *Object_data(Object *  self) {
  return self + 1;
}

inline Object *super(void *  self) {
  return ((Object *)self) - 1;  
}

inline void Object_retain(Object *  self) {
//  printf("RETAIN!!! %d\n", self->type);
#ifdef REFCOUNT_TRACING
  atomic_fetch_add_explicit(&(allocationCount[self->type-1]), 1, memory_order_relaxed);
#endif
#ifdef REFCOUNT_NONATOMIC
  self->refCount++;
#else    
  atomic_fetch_add_explicit(&(self->atomicRefCount), 1, memory_order_relaxed);
#endif
}

inline void Object_destroy(Object * self, BOOL deallocateChildren) {
//  printf("--> Deallocating type %d addres %lld\n", self->type, (uint64_t)Object_data(self));
 // printReferenceCounts();
  switch((objectType)self->type) {
  case integerType:
    Integer_destroy((Integer *)Object_data(self));
    break;          
  case stringType:
    String_destroy((String *)Object_data(self));
    break;          
  case persistentListType:
    PersistentList_destroy((PersistentList *)Object_data(self), deallocateChildren);
    break;          
  case persistentVectorType:
    PersistentVector_destroy((PersistentVector *)Object_data(self), deallocateChildren);
    break;
  case persistentVectorNodeType:
    PersistentVectorNode_destroy((PersistentVectorNode *)Object_data(self), deallocateChildren);
    break;
  case doubleType:
    Double_destroy((Double *)Object_data(self));
    break;          
  case booleanType:
    Boolean_destroy((Boolean *)Object_data(self));
    break;
  case symbolType:
    Symbol_destroy((Symbol *)Object_data(self));
    break;
  case classType:
    Class_destroy((Class *)Object_data(self));
    break;
  case deftypeType:
    Deftype_destroy((Deftype *)Object_data(self));
    break;
  case nilType:
    Nil_destroy((Nil *)Object_data(self));
    break;
  case concurrentHashMapType:
    ConcurrentHashMap_destroy((ConcurrentHashMap *)Object_data(self));
    break;
  case keywordType:
    Keyword_destroy((Keyword *)Object_data(self));
    break;
  case functionType:
    Function_destroy((ClojureFunction *)Object_data(self));
    break;
  case varType:
    Var_destroy((Var *)Object_data(self));
    break;
  case bigIntegerType:
    BigInteger_destroy((BigInteger *)Object_data(self));
    break;
  case ratioType:
    Ratio_destroy((Ratio *)Object_data(self));
    break;
  case persistentArrayMapType:
    PersistentArrayMap_destroy((PersistentArrayMap *)Object_data(self), deallocateChildren);
    break;
  }
  deallocate(self);
 // printf("dealloc end %lld\n", (uint64_t)Object_data(self));
 // printReferenceCounts();
 // printf("=========================\n");
}

inline BOOL Object_isReusable(Object * self) {
  uint64_t refCount = atomic_load_explicit(&(self->atomicRefCount), memory_order_relaxed);
  // Multithreading - is it really safe to assume it is reusable if refcount is 1? 
  /*  The reasoning here is that passing object to another thread is an operation that by 
      itself increases its reference count. Therefore it is assumed that if recount is 1 at 
      a point in time this means other threads have no knowledge of this object's existence 
      at this particular moment. Therefore, if only our thread knows of it, it can pass it to another 
      but only after it completes current operation.
      */
  return refCount == 1;
}

inline BOOL isReusable(void * self) {
  return Object_isReusable(super(self));
}

inline BOOL Object_release_internal(Object *  self, BOOL deallocateChildren) {
#ifdef REFCOUNT_TRACING
//    printf("RELEASE!!! %p %d %d\n", self, self->type, deallocateChildren);
    atomic_fetch_sub_explicit(&(allocationCount[self->type -1 ]), 1, memory_order_relaxed);
#ifndef REFCOUNT_NONATOMIC
    assert(atomic_load(&(self->atomicRefCount)) > 0);
#else
    assert(self->refCount > 0);
#endif
#endif
#ifdef REFCOUNT_NONATOMIC
  if (--self->refCount == 0) {
#else
  uint64_t relVal = atomic_fetch_sub_explicit(&(self->atomicRefCount), 1, memory_order_relaxed);  
  assert(relVal >= 1 && "Memory corruption!");
  if (relVal == 1) {
#ifdef REFCOUNT_TRACING
    uint64_t countVal = atomic_fetch_sub_explicit(&(objectCount[self->type -1 ]), 1, memory_order_relaxed);
    assert(countVal >= 1 && "Memory corruption!");
#endif
#endif
    Object_destroy(self, deallocateChildren);
    return TRUE;
  }
  return FALSE;
}


/* Outside of refcount system */
inline uint64_t Object_hash(Object *  self) {
      switch((objectType)self->type) {
      case integerType:
        return Integer_hash((Integer *)Object_data(self));
        break;          
      case stringType:
        return String_hash((String *)Object_data(self));
        break;          
      case persistentListType:
        return PersistentList_hash((PersistentList *)Object_data(self));
        break;          
      case persistentVectorType:
        return PersistentVector_hash((PersistentVector *)Object_data(self));
        break;
      case persistentVectorNodeType:
        return PersistentVectorNode_hash((PersistentVectorNode *)Object_data(self));
        break;
      case doubleType:
        return Double_hash((Double *)Object_data(self));
        break;          
      case booleanType:
        return Boolean_hash((Boolean *)Object_data(self));
        break;
      case nilType:
        return Nil_hash((Nil *)Object_data(self));
        break;
      case symbolType:
        return Symbol_hash((Symbol *)Object_data(self));
      case classType:
        return Class_hash((Class *)Object_data(self));
      case deftypeType:
        return Deftype_hash((Deftype *)Object_data(self));
      case concurrentHashMapType:
        return ConcurrentHashMap_hash((ConcurrentHashMap *)Object_data(self));
      case keywordType:
        return Keyword_hash((Keyword *)Object_data(self));
      case functionType:
        return Function_hash((ClojureFunction *)Object_data(self));
      case varType:
        return Var_hash((Var *)Object_data(self));
      case bigIntegerType:
        return BigInteger_hash((BigInteger *)Object_data(self));
      case ratioType:
        return Ratio_hash((Ratio *)Object_data(self));
      case persistentArrayMapType:
        return PersistentArrayMap_hash((PersistentArrayMap *)Object_data(self));        
      }
}

/* Outside of refcount system */
inline uint64_t hash(void *  self) {
  return Object_hash(super(self));
}

/* Outside of refcount system */
inline BOOL Object_equals(Object *  self, Object *  other) {
  if (self == other) return TRUE;
  if (self->type != other->type) return FALSE;
  if (Object_hash(self) != Object_hash(other)) return FALSE;

  void *selfData = Object_data(self);
  void *otherData = Object_data(other);  

  switch((objectType)self->type) {
  case integerType:
    return Integer_equals((Integer *)selfData, (Integer *)otherData);
    break;          
  case stringType:
    return String_equals((String *)selfData, (String *)otherData);
    break;          
  case persistentListType:
    return PersistentList_equals((PersistentList *)selfData, (PersistentList *)otherData);
    break;          
  case persistentVectorType:
    return PersistentVector_equals((PersistentVector *)selfData, (PersistentVector *)otherData);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_equals((PersistentVectorNode *)selfData, (PersistentVectorNode *)otherData);
    break;
  case doubleType:
    return Double_equals((Double *)selfData, (Double *)otherData);
    break;
  case booleanType:
    return Boolean_equals((Boolean *)selfData, (Boolean *)otherData);
    break;
  case nilType:
    return Nil_equals((Nil *)selfData, (Nil *)otherData);
    break;
  case symbolType:
    return Symbol_equals((Symbol *)selfData, (Symbol *)otherData);
    break;
  case classType:
    return Class_equals((Class *)selfData, (Class *)otherData);
    break;
  case deftypeType:
    return Deftype_equals((Deftype *)selfData, (Deftype *)otherData);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_equals((ConcurrentHashMap *)selfData, (ConcurrentHashMap *)otherData);
    break;
  case keywordType:
    return Keyword_equals((Keyword *)selfData, (Keyword *)otherData);
    break;
  case functionType:
    return Function_equals((ClojureFunction *)selfData, (ClojureFunction *)otherData);
    break;
  case varType:
    return Var_equals((Var *)selfData, (Var *)otherData);
    break;
  case bigIntegerType:
    return BigInteger_equals((BigInteger *)selfData, (BigInteger *)otherData);
    break;
  case ratioType:
    return Ratio_equals((Ratio *)selfData, (Ratio *)otherData);
    break;
  case persistentArrayMapType:
    return PersistentArrayMap_equals((PersistentArrayMap *)selfData, (PersistentArrayMap *)otherData);
    break;
  }
}

/* Outside of refcount system */
inline BOOL equals(void *  self, void *  other) {
  return Object_equals(super(self), super(other));
}


inline String *Object_toString(Object *  self) {
  switch((objectType)self->type) {
  case integerType:
    return Integer_toString((Integer *)Object_data(self));
    break;          
  case stringType:
    return String_toString((String *)Object_data(self));
    break;          
  case persistentListType:
    return PersistentList_toString((PersistentList *)Object_data(self));
    break;          
  case persistentVectorType:
    return PersistentVector_toString((PersistentVector *)Object_data(self));
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_toString((PersistentVectorNode *)Object_data(self));
    break;
  case doubleType:
    return Double_toString((Double *)Object_data(self));
    break;
  case booleanType:
    return Boolean_toString((Boolean *)Object_data(self));
    break;
  case nilType:
    return Nil_toString((Nil *)Object_data(self));
    break;
  case symbolType:
    return Symbol_toString((Symbol *)Object_data(self));
  case classType:
    return Class_toString((Class *)Object_data(self));
  case deftypeType:
    return Deftype_toString((Deftype *)Object_data(self));
  case concurrentHashMapType:
    return ConcurrentHashMap_toString((ConcurrentHashMap *)Object_data(self));
  case keywordType:
    return Keyword_toString((Keyword *)Object_data(self));
  case functionType:
    return Function_toString((ClojureFunction *)Object_data(self));
  case varType:
    return Var_toString((Var *)Object_data(self));
  case bigIntegerType:
    return BigInteger_toString((BigInteger *)Object_data(self));
  case ratioType:
    return Ratio_toString((Ratio *)Object_data(self));
  case persistentArrayMapType:
    return PersistentArrayMap_toString((PersistentArrayMap *)Object_data(self));
  }
}

inline String *toString(void *  self) {
  return Object_toString(super(self));
}

inline BOOL Object_release(Object *  self) {
  return Object_release_internal(self, TRUE);
}

/* Outside of refcount system */
inline objectType getType(void *obj) {
  return super(obj)->type;
}

inline void Object_autorelease(Object *  self) {
  /* The object could have been deallocated through direct releases in the meantime (e.g. if autoreleasing entity does not own ) */
#ifdef REFCOUNT_NONATOMIC
  if(self->refCount < 1) return;   
#else
  if(atomic_load(&(self->atomicRefCount)) < 1) return;   
#endif
  /* TODO: add an object to autorelease pool */
  /* If we have no other threads working, we release immediately */
  Object_release(self);
}

inline void autorelease(void *  self) {
  Object_autorelease(super(self));
}

inline void retain(void *  self) {
  Object_retain(super(self));
}

inline BOOL release(void *  self) {
   return Object_release(super(self));
}

inline void Object_create(Object *  self, objectType type) {
#ifdef REFCOUNT_NONATOMIC
  self->refCount = 1;
#else
  atomic_store_explicit (&(self->atomicRefCount), 1, memory_order_relaxed);
#endif
  self->type = type;
#ifdef REFCOUNT_TRACING
  atomic_fetch_add_explicit(&(allocationCount[self->type-1]), 1, memory_order_relaxed);
  atomic_fetch_add_explicit(&(objectCount[self->type-1]), 1, memory_order_relaxed);
#endif
//  printf("--> Allocating type %d addres %p\n", self->type, Object_data(self));
}


inline uint64_t combineHash(uint64_t lhs, uint64_t rhs) {
  lhs ^= rhs + 0x9ddfea08eb382d69ULL + (lhs << 6) + (lhs >> 2);
  return lhs;
}
#pragma clang diagnostic pop

#endif
