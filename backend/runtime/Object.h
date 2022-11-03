#ifndef RT_OBJECT
#define RT_OBJECT

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
#include <assert.h>
#include <execinfo.h>

typedef struct String String; 

//#define REFCOUNT_TRACING

struct Object {
  objectType type;
  volatile atomic_uint_fast64_t refCount;
};

typedef struct Object Object; 

extern _Atomic uint64_t allocationCount[10]; 

void initialise_memory();

inline void *allocate(size_t size) {
  /* if (size <= 32) return malloc(size);    */
  /* if (size <= 64) return poolMalloc(&globalPool3);    */
  /* if (size <= 128) return poolMalloc(&globalPool2);  */
  /* if (size > 64 && size <= BLOCK_SIZE) return poolMalloc(&globalPool1);    */
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

inline void *data(Object * restrict self) {
  return self + 1;
}

inline Object *super(void * restrict self) {
  return (Object *)(self - sizeof(Object));
}

inline void Object_retain(Object * restrict self) {
  #ifdef REFCOUNT_TRACING
  atomic_fetch_add_explicit(&(allocationCount[self->type]), 1, memory_order_relaxed);
  #endif
  atomic_fetch_add_explicit(&(self->refCount), 1, memory_order_relaxed);
}

inline BOOL Object_release_internal(Object * restrict self, BOOL deallocateChildren) {
  #ifdef REFCOUNT_TRACING
    atomic_fetch_sub_explicit(&(allocationCount[self->type]), 1, memory_order_relaxed);
    assert(atomic_load(&(self->refCount)) > 0);
  #endif

  if (atomic_fetch_sub_explicit(&(self->refCount), 1, memory_order_relaxed) == 1) {
    switch((objectType)self->type) {
    case integerType:
      Integer_destroy(data(self));
      break;          
    case stringType:
      String_destroy(data(self));
      break;          
    case persistentListType:
      PersistentList_destroy(data(self), deallocateChildren);
      break;          
    case persistentVectorType:
      PersistentVector_destroy(data(self), deallocateChildren);
      break;
    case persistentVectorNodeType:
      PersistentVectorNode_destroy(data(self), deallocateChildren);
      break;
    case doubleType:
      Double_destroy(data(self));
      break;          
    case booleanType:
      Boolean_destroy(data(self));
      break;
    case symbolType:
      Symbol_destroy(data(self));
      break;
    case nilType:
      Nil_destroy(data(self));
      break;
    case concurrentHashMapType:
      ConcurrentHashMap_destroy(data(self));
      break;
    case keywordType:
      Keyword_destroy(data(self));
      break;
    }
    deallocate(self);
    return TRUE;
  }
  return FALSE;
}

inline uint64_t hash(Object * restrict self) {
      switch((objectType)self->type) {
      case integerType:
        return Integer_hash(data(self));
        break;          
      case stringType:
        return String_hash(data(self));
        break;          
      case persistentListType:
        return PersistentList_hash(data(self));
        break;          
      case persistentVectorType:
        return PersistentVector_hash(data(self));
        break;
      case persistentVectorNodeType:
        return PersistentVector_hash(data(self));
        break;
      case doubleType:
        return Double_hash(data(self));
        break;          
      case booleanType:
        return Boolean_hash(data(self));
        break;
      case nilType:
        return Nil_hash(data(self));
        break;
      case symbolType:
        return Symbol_hash(data(self));
      case concurrentHashMapType:
        return ConcurrentHashMap_hash(data(self));
      case keywordType:
        return Keyword_hash(data(self));
      }
}

inline BOOL equals(Object * restrict self, Object * restrict other) {
  if (self == other) return TRUE;
  if (self->type != other->type) return FALSE;
  if (hash(self) != hash(other)) return FALSE;

  void *selfData = data(self);
  void *otherData = data(other);  

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
  case concurrentHashMapType:
    return ConcurrentHashMap_equals(selfData, otherData);
    break;
  case keywordType:
    return Keyword_equals(selfData, otherData);
    break;
  }
}

inline String *toString(Object * restrict self) {
  switch((objectType)self->type) {
  case integerType:
    return Integer_toString(data(self));
    break;          
  case stringType:
    return String_toString(data(self));
    break;          
  case persistentListType:
    return PersistentList_toString(data(self));
    break;          
  case persistentVectorType:
    return PersistentVector_toString(data(self));
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_toString(data(self));
    break;
  case doubleType:
    return Double_toString(data(self));
    break;
  case booleanType:
    return Boolean_toString(data(self));
    break;
  case nilType:
    return Nil_toString(data(self));
    break;
  case symbolType:
    return Symbol_toString(data(self));
  case concurrentHashMapType:
    return ConcurrentHashMap_toString(data(self));
  case keywordType:
    return Keyword_toString(data(self));
  }
}

inline BOOL Object_release(Object * restrict self) {
  return Object_release_internal(self, TRUE);
}

inline void Object_autorelease(Object * restrict self) {
  /* TODO: add an object to autorelease pool */

}

inline void retain(void * restrict self) {
  Object_retain(super(self));
}

inline BOOL release(void * restrict self) {
   return Object_release(super(self));
}

inline void Object_create(Object * restrict self, objectType type) {
  atomic_store_explicit (&(self->refCount), 1, memory_order_relaxed);
  self->type = type;
#ifdef REFCOUNT_TRACING
  atomic_fetch_add_explicit(&(allocationCount[self->type]), 1, memory_order_relaxed);
#endif
}


inline uint64_t combineHash(uint64_t lhs, uint64_t rhs) {
  lhs ^= rhs + 0x9ddfea08eb382d69ULL + (lhs << 6) + (lhs >> 2);
  return lhs;
}

#endif
