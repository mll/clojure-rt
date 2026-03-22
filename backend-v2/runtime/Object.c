#include "Object.h"
#include "RTValue.h"
#include "RuntimeInterface.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

_Atomic(uword_t) allocationCount[256];
_Atomic(uword_t) objectCount[256];
#ifndef COMPILING_RUNTIME_BITCODE
_Thread_local void *memoryBank[8] = {0};
_Thread_local int memoryBankSize[8] = {0};
#endif

/* pool globalPool1; */
/* pool globalPool2; */
/* pool globalPool3; */

/* Must be a multiple of processor word, 8 on 64 bit, 4 on 32 bit */

#define BLOCK_SIZE 8 * 40

void PersistentVector_initialise();
void PersistentArrayMap_initialise();

void initialise_memory() {
  for (int i = 0; i < 200; i++)
    atomic_exchange(&(allocationCount[i]), 0);
  /* poolInitialize(&globalPool1, BLOCK_SIZE, 100000); */
  /* poolInitialize(&globalPool2, 128, 100000); */
  /* poolInitialize(&globalPool3, 64, 100000); */
  PersistentVector_initialise();
  PersistentArrayMap_initialise();
  RuntimeInterface_initialise();
}

/* BOOL poolFreeCheck(void *ptr, pool *mempool) { */
/*   uint8_t *rptr = ptr; */
/*   for(int i=0; i< mempool->blocksUsed; i++) { */
/*     uint8_t *bptr = mempool->blocks[i]; */
/*     uint8_t *fptr = bptr + mempool->blockSize * mempool->elementSize; */
/*     if(bptr == NULL) continue; */
/*     if(rptr >= bptr && rptr < fptr) { */
/*       return TRUE; */
/*     } */
/*   } */
/*   return FALSE; */
/* } */

extern void *allocate(size_t size);
extern void deallocate(void *restrict ptr);

void retain(RTValue self);
bool release(RTValue self);
extern void autorelease(RTValue self);
extern bool release_internal(void *restrict self, bool deallocatesChildren);
bool equals(RTValue self, RTValue other);
extern uword_t hash(RTValue self);
String *toString(RTValue self);

extern void Object_create(Object *restrict self, objectType type);
extern void Object_retain(Object *restrict self);
extern bool Object_release(Object *restrict self);
extern bool Object_release_internal(Object *restrict self,
                                    bool deallocatesChildren);
extern void Object_destroy(Object *restrict self, bool deallocateChildren);
extern void Object_autorelease(Object *restrict self);
extern bool Object_equals(Object *restrict self, Object *restrict other);
extern uword_t Object_hash(Object *restrict self);
extern String *Object_toString(Object *restrict self);
extern bool Object_isReusable(Object *restrict self);
bool isReusable(RTValue self);
objectType getType(RTValue obj);

extern uword_t combineHash(uword_t lhs, uword_t rhs);

extern void Ptr_autorelease(void *self);
void Ptr_retain(void *self);
bool Ptr_release(void *self);
extern uword_t Ptr_hash(void *self);
extern bool Ptr_equals(void *self, void *other);
extern bool Ptr_isReusable(void *self);
bool equals_managed(RTValue self, RTValue other);
extern void Object_promoteToShared(Object *restrict self);
extern void Object_promoteToSharedShallow(Object *restrict self,
                                          uword_t current);
extern uword_t Object_getRawRefCount(Object *self);
extern void promoteToShared(RTValue self);
