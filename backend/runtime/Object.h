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
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"


typedef struct String String; 

typedef enum objectType objectType;

struct Object {
  objectType type;
  volatile atomic_uint_fast64_t refCount;
};

typedef struct Object Object; 

void initialise_memory();


inline void *allocate(size_t size) {
  /* if (size <= 32) return malloc(size);    */
  /* if (size <= 64) return poolMalloc(&globalPool3);    */
  /* if (size <= 128) return poolMalloc(&globalPool2);  */
  /* if (size > 64 && size <= BLOCK_SIZE) return poolMalloc(&globalPool1);    */

  return malloc(size);  
}

inline void deallocate(void *ptr) {
 /* if (poolFreeCheck(ptr, &globalPool3)) { poolFree(&globalPool3, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool2)) { poolFree(&globalPool2, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool1)) { poolFree(&globalPool1, ptr); return; }  */
  free(ptr);  
}

inline void *data(Object *self) {
  return self + 1;
}

inline Object *super(void *self) {
  return (Object *)(self - sizeof(Object));
}

inline void Object_retain(Object *self) {
  /* Just for single thread debug, nonatomic */
 // return;
//  allocationCount[self->type]++;
  atomic_fetch_add(&(self->refCount), 1);
}

inline BOOL Object_release_internal(Object *self, BOOL deallocateChildren) {
  /* Just for single thread debug, nonatomic */
  //return FALSE;
  //allocationCount[self->type]--;
  //assert(atomic_load(&(self->refCount)) > 0);
  if (atomic_fetch_sub(&(self->refCount), 1) == 1) {
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

    }
    deallocate(self);
    return TRUE;
  }
  return FALSE;
}

inline BOOL Object_release(Object *self) {
  //return FALSE;
  return Object_release_internal(self, TRUE);
}

inline void retain(void *self) {
  //return;
  Object_retain(super(self));
}

inline BOOL release(void *self) {
   //return FALSE; 
   return Object_release(super(self));
}

inline void Object_create(Object *self, objectType type) {
  atomic_store_explicit (&(self->refCount), 1, memory_order_relaxed);
  self->type = type;
  /* Just for single thread debug, nonatomic */
//  allocationCount[self->type]++;
}

inline BOOL equals(Object *self, Object *other) {
  void *selfData = data(self);
  void *otherData = data(other);
  if (self == other || selfData == otherData) return TRUE;
  if (self->type != other->type) return FALSE;
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
  }
}

inline uint64_t hash(Object *self) {
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
      }
}

inline String *toString(Object *self) {
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

      }
}


#endif
