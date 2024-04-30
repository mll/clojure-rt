#include "Object.h"
#include "pool/pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdatomic.h>
#include "Interface.h"

_Atomic uint64_t allocationCount[256]; 
_Atomic uint64_t objectCount[256];
/* pool globalPool1; */
/* pool globalPool2; */
/* pool globalPool3; */

/* Must be a multiple of processor word, 8 on 64 bit, 4 on 32 bit */

#define BLOCK_SIZE 8 * 40

void PersistentVector_initialise();
void Nil_initialise();

void initialise_memory() {
  for(int i=0; i<200; i++) atomic_exchange(&(allocationCount[i]), 0); 
  /* poolInitialize(&globalPool1, BLOCK_SIZE, 100000); */
  /* poolInitialize(&globalPool2, 128, 100000); */
  /* poolInitialize(&globalPool3, 64, 100000); */
  PersistentVector_initialise();
  Nil_initialise();
  Interface_initialise();
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
extern void deallocate(void * restrict ptr);

extern Object *super(void * restrict self);
extern void retain(void * restrict self);
extern BOOL release(void * restrict self);
extern void autorelease(void * restrict self);
extern BOOL release_internal(void * restrict self, BOOL deallocatesChildren);
extern BOOL equals(void * restrict self, void * restrict other);
extern uint64_t hash(void * restrict self);
extern String *toString(void * restrict self);

extern void *Object_data(Object * restrict self);
extern void Object_create(Object * restrict self, objectType type);
extern void Object_retain(Object * restrict self);
extern BOOL Object_release(Object * restrict self);
extern BOOL Object_release_internal(Object * restrict self, BOOL deallocatesChildren);
extern void Object_destroy(Object *restrict self, BOOL deallocateChildren);
extern void Object_autorelease(Object * restrict self);
extern BOOL Object_equals(Object * restrict self, Object * restrict other);
extern uint64_t Object_hash(Object * restrict self);
extern String *Object_toString(Object * restrict self);
extern BOOL Object_isReusable(Object *restrict self);
extern BOOL isReusable(void *restrict self);
extern objectType getType(void *obj);

extern uint64_t combineHash(uint64_t lhs, uint64_t rhs);


