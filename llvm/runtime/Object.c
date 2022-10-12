#include "Object.h"
#include "String.h"
#include "Integer.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "pool/pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdatomic.h>

int allocationCount[5];
pool globalPool1;
pool globalPool2;
pool globalPool3;



/* Must be a multiple of processor word, 8 on 64 bit, 4 on 32 bit */

#define BLOCK_SIZE 8 * 40

void PersistentVector_initialise();

void initialise_memory() {
  memset(allocationCount, 0, sizeof(int)*5);
  poolInitialize(&globalPool1, BLOCK_SIZE, 100000);
  poolInitialize(&globalPool2, 128, 100000);
  poolInitialize(&globalPool3, 64, 100000);
  PersistentVector_initialise();
}

bool poolFreeCheck(void *ptr, pool *mempool) {
  uint8_t *rptr = ptr;
  for(int i=0; i< mempool->blocksUsed; i++) {
    uint8_t *bptr = mempool->blocks[i];
    uint8_t *fptr = bptr + mempool->blockSize * mempool->elementSize;
    if(bptr == NULL) continue;
    if(rptr >= bptr && rptr < fptr) {
      return true;
    }
  }
  return false;
}

void *allocate(size_t size) {
  /* if (size <= 32) return malloc(size);    */
  /* if (size <= 64) return poolMalloc(&globalPool3);    */
  /* if (size <= 128) return poolMalloc(&globalPool2);  */
  /* if (size > 64 && size <= BLOCK_SIZE) return poolMalloc(&globalPool1);    */

  return malloc(size);  
}

void deallocate(void *ptr) {
 /* if (poolFreeCheck(ptr, &globalPool3)) { poolFree(&globalPool3, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool2)) { poolFree(&globalPool2, ptr); return; } */
 /* if (poolFreeCheck(ptr, &globalPool1)) { poolFree(&globalPool1, ptr); return; }  */
  free(ptr);  
}

void *data(Object *self) {
  return self + 1;
}

Object *super(void *self) {
  return (Object *)(self - sizeof(Object));
}


void Object_create(Object *self, objectType type) {
  atomic_store_explicit (&(self->refCount), 1, memory_order_relaxed);
  self->type = type;
  /* Just for single thread debug, nonatomic */
  allocationCount[self->type]++;
}

bool equals(Object *self, Object *other) {
  void *selfData = data(self);
  void *otherData = data(other);
  if (self == other || selfData == otherData) return true;
  if (self->type != other->type) return false;
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
  }
}

uint64_t hash(Object *self) {
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
      }
}

String *toString(Object *self) {
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
      }
}

void retain(void *self) {
  Object_retain(super(self));
}

bool release(void *self) {
  return Object_release(super(self));
}

void Object_retain(Object *self) {
  /* Just for single thread debug, nonatomic */
//  allocationCount[self->type]++;
  atomic_fetch_add(&(self->refCount), 1);
}

bool Object_release_internal(Object *self, bool deallocateChildren) {
  /* Just for single thread debug, nonatomic */
  //return false;
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
    }
    deallocate(self);
    return true;
  }
  return false;
}

bool Object_release(Object *self) {
  return Object_release_internal(self, true);
}

