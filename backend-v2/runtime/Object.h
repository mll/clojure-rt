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
#include <assert.h>
#include <execinfo.h>
#include "RTValue.h"
#include "defines.h"
#include "PersistentVectorIterator.h"
#include <stdatomic.h>
#include "String.h"
#include "Integer.h"
#include "Double.h"
#include "Boolean.h"
#include "Nil.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "PersistentVectorIterator.h"
#include "ConcurrentHashMap.h"
#include "Symbol.h"
#include "Keyword.h"
#include "Function.h"
#include "BigInteger.h"
#include "Ratio.h"
#include "Class.h"
#include "PersistentArrayMap.h"

typedef struct String String; 

#define MEMORY_BANK_SIZE_MAX 10

extern void logBacktrace();
void printReferenceCounts();

extern _Atomic uword_t allocationCount[256]; 
extern _Atomic uword_t objectCount[256]; 

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
#else 
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
#endif
}

inline void deallocate(void *ptr) {
#ifndef USE_MEMORY_BANKS
  free(ptr);
  return;
#else
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
#endif
}

/* Outside of refcount system */
inline objectType getType(RTValue v) {
  if (RT_isDouble(v))  return doubleType;
  if (RT_isInt32(v))   return integerType;
  if (RT_isBool(v))    return booleanType;
  if (RT_isNil(v))     return nilType;
  if (RT_isKeyword(v)) return keywordType;
  if (RT_isSymbol(v))  return symbolType;
  assert(RT_isPtr(v) && "Internal error: Not a pointer");
  return ((Object*)RT_unboxPtr(v))->type;  
}

inline void Object_retain(Object *restrict self) {
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

inline void Object_destroy(Object *restrict self, bool deallocateChildren) {
//  printf("--> Deallocating type %d addres %lld\n", self->type, (uword_t));
 // printReferenceCounts();
  switch((objectType)self->type) {
  case stringType:
    String_destroy((String *) self);
    break;
  case classType:
    Class_destroy((Class *) self);
    break;
  case interfaceType:
    Interface_destroy((Interface *) self);
    break;              
  case persistentListType:
    PersistentList_destroy((PersistentList *)self, deallocateChildren);
    break;          
  case persistentVectorType:
    PersistentVector_destroy((PersistentVector *)self, deallocateChildren);
    break;
  case persistentVectorNodeType:
    PersistentVectorNode_destroy((PersistentVectorNode *)self, deallocateChildren);
    break;
  case concurrentHashMapType:
    ConcurrentHashMap_destroy((ConcurrentHashMap *) self);
    break;
  case functionType:
    Function_destroy((ClojureFunction *) self);
    break;
  case bigIntegerType:
    BigInteger_destroy((BigInteger *) self);
    break;
  case ratioType:
    Ratio_destroy((Ratio *) self);
    break;
  case persistentArrayMapType:
    PersistentArrayMap_destroy((PersistentArrayMap *)self, deallocateChildren);
    break;
  default:
    assert(false && "Internal error: hash computation for NaN tagged types should be computed earlier.");    
  }
  deallocate(self);
 // printf("dealloc end %lld\n", (uword_t));
 // printReferenceCounts();
 // printf("=========================\n");
}

inline bool Object_isReusable(Object *restrict self) {
  uword_t refCount = atomic_load_explicit(&(self->atomicRefCount), memory_order_acquire);
  // Multithreading - is it really safe to assume it is reusable if refcount is 1? 
  /*  The reasoning here is that passing object to another thread is an operation that by 
      itself increases its reference count. Therefore it is assumed that if recount is 1 at 
      a point in time this means other threads have no knowledge of this object's existence 
      at this particular moment. Therefore, if only our thread knows of it, it can pass it to another 
      but only after it completes current operation.
      */
  return refCount == 1;
}

inline bool isReusable(RTValue self) {
  return RT_isPtr(self) && Object_isReusable((Object *)RT_unboxPtr(self));
}

inline bool Object_release_internal(Object *restrict self, bool deallocateChildren) {
#ifdef REFCOUNT_TRACING
//    printf("RELEASE!!! %p %d %d\n", self, self->type, deallocateChildren);
    atomic_fetch_sub_explicit(&(allocationCount[self->type - 1]), 1, memory_order_relaxed);
#ifndef REFCOUNT_NONATOMIC
    assert(atomic_load(&(self->atomicRefCount)) > 0);
#else
    assert(self->refCount > 0);
#endif
#endif
#ifdef REFCOUNT_NONATOMIC
  if (--self->refCount == 0) {
#else
  uword_t relVal = atomic_fetch_sub_explicit(&(self->atomicRefCount), 1, memory_order_release);  
  assert(relVal >= 1 && "Memory corruption!");
  if (relVal == 1) {
#ifdef REFCOUNT_TRACING
    uword_t countVal = atomic_fetch_sub_explicit(&(objectCount[self->type -1 ]), 1, memory_order_relaxed);
    assert(countVal >= 1 && "Memory corruption!");
#endif
#endif
    atomic_thread_fence(memory_order_acquire);    
    Object_destroy(self, deallocateChildren);
    return true;
  }
  return false;
}


/* Outside of refcount system */
inline uword_t Object_hash(Object *restrict self) {
      switch((objectType)self->type) {
      case stringType:
        return String_hash((String *) self);
        break;
      case classType:
        return Class_hash((Class *)self);
        break;
      case interfaceType:
        return Interface_hash((Interface *)self);
        break;        
      case persistentListType:
        return PersistentList_hash((PersistentList *) self);
        break;          
      case persistentVectorType:
        return PersistentVector_hash((PersistentVector *) self);
        break;
      case persistentVectorNodeType:
        return PersistentVectorNode_hash((PersistentVectorNode *) self);
        break;
      case concurrentHashMapType:
        return ConcurrentHashMap_hash((ConcurrentHashMap *) self);
      case functionType:
        return Function_hash((ClojureFunction *) self);
      case bigIntegerType:
        return BigInteger_hash((BigInteger *) self);
      case ratioType:
        return Ratio_hash((Ratio *) self);
      case persistentArrayMapType:
        return PersistentArrayMap_hash((PersistentArrayMap *)self);
      default:
        assert(false && "Internal error: hash computation for NaN tagged types should be computed earlier.");
      }
}

/* Outside of refcount system */
inline uword_t hash(RTValue v) {
    objectType t = getType(v);
    switch(t) {
        case integerType: return (uword_t)RT_unboxInt32(v) + 1;
        case booleanType: return (uword_t)RT_unboxBool(v) + 1;
        case nilType:     return 0xDEADBEEF; // Standard nil hash
        case keywordType: return (uword_t)RT_unboxKeyword(v);
        case symbolType:  return (uword_t)RT_unboxSymbol(v);        
        case doubleType: {
            double d = RT_unboxDouble(v);
            uint64_t u;
            memcpy(&u, &d, 8);
            return u + 1;
        }
        default:
          assert(RT_isPtr(v) && "Internal error - trying to compute hash from an unknown value");            
          return Object_hash((Object*)RT_unboxPtr(v));
    }
}

/* Outside of refcount system */
inline bool Object_equals(Object *self, Object *other) {
  if (Object_hash(self) != Object_hash(other)) return false;

  switch((objectType)self->type) {
  case stringType:
    return String_equals((String *)self, (String *)other);
    break;
  case classType:
    return Class_equals((Class *)self, (Class *)other);
    break;
  case interfaceType:
    return Interface_equals((Interface *)self, (Interface *)other);
    break;    
  case persistentListType:
    return PersistentList_equals((PersistentList *)self, (PersistentList *)other);
    break;          
  case persistentVectorType:
    return PersistentVector_equals((PersistentVector *)self, (PersistentVector *)other);
    break;
  case persistentVectorNodeType:
    return PersistentVectorNode_equals((PersistentVectorNode *)self, (PersistentVectorNode *)other);
    break;
  case concurrentHashMapType:
    return ConcurrentHashMap_equals((ConcurrentHashMap *)self, (ConcurrentHashMap *)other);
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
    return PersistentArrayMap_equals((PersistentArrayMap *)self, (PersistentArrayMap *)other);
    break;
  default:
    assert(false && "Internal error: hash computation for NaN tagged types should be computed earlier.");    
  }
}

/* Outside of refcount system */
inline bool equals(RTValue self, RTValue other) {
  if (self == other) return true; // Handles same-ints, same-bools, and same-pointers
  
  objectType t1 = getType(self);
  objectType t2 = getType(other);
  if (t1 != t2) return false;
  
  if (RT_isPtr(self)) {
    return Object_equals((Object*)RT_unboxPtr(self), (Object*)RT_unboxPtr(other));
  }
  
  if (t1 == doubleType) {
    return RT_unboxDouble(self) == RT_unboxDouble(other);
  }
  return false;  
}


inline String *Object_toString(Object *restrict self) {
  switch((objectType)self->type) {
  case stringType:
    return String_toString((String *)self);
    break;
  case classType:
    return Class_toString((Class *)self);
    break;
  case interfaceType:
    return Interface_toString((Interface *)self);
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
  default:
    assert(false && "Internal error: Object_toString got an unsupported type");
  }
}

inline String *toString(RTValue self) {
  if (RT_isInt32(self))  return Integer_toString(self);
  if (RT_isDouble(self)) return Double_toString(self);
  if (RT_isBool(self)) return Boolean_toString(self);
  if (RT_isNil(self)) return Nil_toString();  
  if (RT_isKeyword(self)) return Keyword_toString(self);  
  if (RT_isSymbol(self)) return Symbol_toString(self);

  assert(RT_isPtr(self) && "Internal error: Not a pointer");
  return Object_toString((Object*)RT_unboxPtr(self));  
}

inline bool Object_release(Object *restrict self) {
  return Object_release_internal(self, true);
}

inline void Object_autorelease(Object *restrict self) {
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

inline void autorelease(RTValue self) {
  if (RT_isPtr(self)) {
        Object_autorelease((Object*)RT_unboxPtr(self));
  }  
}

inline void retain(RTValue self) {
  if (RT_isPtr(self)) {
        Object_retain((Object*)RT_unboxPtr(self));
  }    
}

inline bool release(RTValue self) {
  if (RT_isPtr(self)) {
    return Object_release((Object*)RT_unboxPtr(self));
  }      
  return false;
}

inline void Object_create(Object *restrict self, objectType type) {
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
//  printf("--> Allocating type %d addres %p\n", self->type, );
}


inline uword_t combineHash(uword_t lhs, uword_t rhs) {
  lhs ^= rhs + 0x9ddfea08eb382d69ULL + (lhs << 6) + (lhs >> 2);
  return lhs;
}

inline void Ptr_autorelease(void *self) { Object_autorelease((Object *)self); }
inline void Ptr_retain(void *self) { Object_retain((Object *)self); }
inline bool Ptr_release(void *self) { return Object_release((Object *)self); }
inline uword_t Ptr_hash(void *self) { return Object_hash((Object*)self); }
inline bool Ptr_isReusable(void *self) { return Object_isReusable((Object *)self); }
inline bool Ptr_equals(void *self, void *other) {
  if (self == other) return true;
  return Object_equals((Object *)self, (Object *)other);
}



#pragma clang diagnostic pop

#endif
