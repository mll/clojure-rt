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

extern void logBacktrace();
void printReferenceCounts();

struct Object {
  objectType type;
#ifdef REFCOUNT_NONATOMIC
  uint64_t refCount;
#else
  volatile atomic_uint_fast64_t atomicRefCount;
#endif
};

typedef struct Object Object; 

extern _Atomic uint64_t allocationCount[256]; 
extern _Atomic uint64_t objectCount[256]; 

void initialise_memory();

inline void *allocate(size_t size) {
  /* if (size <= 32) return malloc(size);    */
  /* if (size <= 64) return poolMalloc(&globalPool3);    */
  /* if (size <= 128) return poolMalloc(&globalPool2);  */
  /* if (size > 64 && size <= BLOCK_SIZE) return poolMalloc(&globalPool1);    */
  assert(size > 0 && "Size = 0");
  void * retVal = malloc(size);  
  assert(retVal && "Could not aloocate that much! ");
  return retVal;
}

inline void deallocate(void * restrict ptr) {
 /* if (poolFreeCheck(ptr, &globalPool3)) { poolFree(&globalPool3, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool2)) { poolFree(&globalPool2, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool1)) { poolFree(&globalPool1, ptr); return; }  */
 free(ptr);  
}

inline void *Object_data(Object * restrict self) {
  return self + 1;
}

inline Object *super(void * restrict self) {
  return (Object *)(self - sizeof(Object));  
}

inline void Object_retain(Object * restrict self) {
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

inline void Object_destroy(Object *restrict self, BOOL deallocateChildren) {
//  printf("--> Deallocating type %d addres %lld\n", self->type, (uint64_t)Object_data(self));
 // printReferenceCounts();
  switch((objectType)self->type) {
  case integerType:
    Integer_destroy(Object_data(self));
    break;          
  case stringType:
    String_destroy(Object_data(self));
    break;          
  case persistentListType:
    PersistentList_destroy(Object_data(self), deallocateChildren);
    break;          
  case persistentVectorType:
    PersistentVector_destroy(Object_data(self), deallocateChildren);
    break;
  case persistentVectorNodeType:
    PersistentVectorNode_destroy(Object_data(self), deallocateChildren);
    break;
  case doubleType:
    Double_destroy(Object_data(self));
    break;          
  case booleanType:
    Boolean_destroy(Object_data(self));
    break;
  case symbolType:
    Symbol_destroy(Object_data(self));
    break;
  case classType:
    Class_destroy(Object_data(self));
    break;
  case deftypeType:
    Deftype_destroy(Object_data(self));
    break;
  case nilType:
    Nil_destroy(Object_data(self));
    break;
  case concurrentHashMapType:
    ConcurrentHashMap_destroy(Object_data(self));
    break;
  case keywordType:
    Keyword_destroy(Object_data(self));
    break;
  case functionType:
    Function_destroy(Object_data(self));
    break;
  case varType:
    Var_destroy(Object_data(self));
    break;
  case bigIntegerType:
    BigInteger_destroy(Object_data(self));
    break;
  case ratioType:
    Ratio_destroy(Object_data(self));
    break;
  case persistentArrayMapType:
    PersistentArrayMap_destroy(Object_data(self), deallocateChildren);
    break;
  }
  deallocate(self);
 // printf("dealloc end %lld\n", (uint64_t)Object_data(self));
 // printReferenceCounts();
 // printf("=========================\n");
}

inline BOOL Object_isReusable(Object *restrict self) {
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

inline BOOL isReusable(void *restrict self) {
  return Object_isReusable(super(self));
}

inline BOOL Object_release_internal(Object * restrict self, BOOL deallocateChildren) {
#ifdef REFCOUNT_TRACING
//    printf("RELEASE!!! %p %d %d\n", self, self->type, deallocateChildren);
    atomic_fetch_sub_explicit(&(allocationCount[self->type -1 ]), 1, memory_order_relaxed);
    assert(atomic_load(&(self->atomicRefCount)) > 0);
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

inline uint64_t Object_hash(Object * restrict self) {
      switch((objectType)self->type) {
      case integerType:
        return Integer_hash(Object_data(self));
        break;          
      case stringType:
        return String_hash(Object_data(self));
        break;          
      case persistentListType:
        return PersistentList_hash(Object_data(self));
        break;          
      case persistentVectorType:
        return PersistentVector_hash(Object_data(self));
        break;
      case persistentVectorNodeType:
        return PersistentVectorNode_hash(Object_data(self));
        break;
      case doubleType:
        return Double_hash(Object_data(self));
        break;          
      case booleanType:
        return Boolean_hash(Object_data(self));
        break;
      case nilType:
        return Nil_hash(Object_data(self));
        break;
      case symbolType:
        return Symbol_hash(Object_data(self));
      case classType:
        return Class_hash(Object_data(self));
      case deftypeType:
        return Deftype_hash(Object_data(self));
      case concurrentHashMapType:
        return ConcurrentHashMap_hash(Object_data(self));
      case keywordType:
        return Keyword_hash(Object_data(self));
      case functionType:
        return Function_hash(Object_data(self));
      case varType:
        return Var_hash(Object_data(self));
      case bigIntegerType:
        return BigInteger_hash(Object_data(self));
      case ratioType:
        return Ratio_hash(Object_data(self));
      case persistentArrayMapType:
        return PersistentArrayMap_hash(Object_data(self));        
      }
}

inline uint64_t hash(void * restrict self) {
  return Object_hash(super(self));
}

inline BOOL Object_equals(Object * restrict self, Object * restrict other) {
  if (self == other) return TRUE;
  if (self->type != other->type) return FALSE;
  if (Object_hash(self) != Object_hash(other)) return FALSE;

  void *selfData = Object_data(self);
  void *otherData = Object_data(other);  

  switch((objectType)self->type) {
  case integerType:
    return Integer_equals(selfData, otherData);
    break;          
  case stringType:
    return String_equals(selfData, otherData);
    break;          
  case persistentListType:
    return PersistentList_equals(selfData, otherData);
    break;          
  case persistentVectorType:
    return PersistentVector_equals(selfData, otherData);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_equals(selfData, otherData);
    break;
  case doubleType:
    return Double_equals(selfData, otherData);
    break;
  case booleanType:
    return Boolean_equals(selfData, otherData);
    break;
  case nilType:
    return Nil_equals(selfData, otherData);
    break;
  case symbolType:
    return Symbol_equals(selfData, otherData);
    break;
  case classType:
    return Class_equals(selfData, otherData);
    break;
  case deftypeType:
    return Deftype_equals(selfData, otherData);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_equals(selfData, otherData);
    break;
  case keywordType:
    return Keyword_equals(selfData, otherData);
    break;
  case functionType:
    return Function_equals(selfData, otherData);
    break;
  case varType:
    return Var_equals(selfData, otherData);
    break;
  case bigIntegerType:
    return BigInteger_equals(selfData, otherData);
    break;
  case ratioType:
    return Ratio_equals(selfData, otherData);
    break;
  case persistentArrayMapType:
    return PersistentArrayMap_equals(selfData, otherData);
    break;
  }
}

inline BOOL equals(void * restrict self, void * restrict other) {
  return Object_equals(super(self), super(other));
}


inline String *Object_toString(Object * restrict self) {
  switch((objectType)self->type) {
  case integerType:
    return Integer_toString(Object_data(self));
    break;          
  case stringType:
    return String_toString(Object_data(self));
    break;          
  case persistentListType:
    return PersistentList_toString(Object_data(self));
    break;          
  case persistentVectorType:
    return PersistentVector_toString(Object_data(self));
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_toString(Object_data(self));
    break;
  case doubleType:
    return Double_toString(Object_data(self));
    break;
  case booleanType:
    return Boolean_toString(Object_data(self));
    break;
  case nilType:
    return Nil_toString(Object_data(self));
    break;
  case symbolType:
    return Symbol_toString(Object_data(self));
  case classType:
    return Class_toString(Object_data(self));
  case deftypeType:
    return Deftype_toString(Object_data(self));
  case concurrentHashMapType:
    return ConcurrentHashMap_toString(Object_data(self));
  case keywordType:
    return Keyword_toString(Object_data(self));
  case functionType:
    return Function_toString(Object_data(self));
  case varType:
    return Var_toString(Object_data(self));
  case bigIntegerType:
    return BigInteger_toString(Object_data(self));
  case ratioType:
    return Ratio_toString(Object_data(self));
  case persistentArrayMapType:
    return PersistentArrayMap_toString(Object_data(self));
  }
}

inline String *toString(void * restrict self) {
  return Object_toString(super(self));
}

inline BOOL Object_release(Object * restrict self) {
  return Object_release_internal(self, TRUE);
}

inline objectType getType(void *obj) {
  return super(obj)->type;
}

inline void Object_autorelease(Object * restrict self) {
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

inline void autorelease(void * restrict self) {
  Object_autorelease(super(self));
}

inline void retain(void * restrict self) {
  Object_retain(super(self));
}

inline BOOL release(void * restrict self) {
   return Object_release(super(self));
}

inline void Object_create(Object * restrict self, objectType type) {
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
