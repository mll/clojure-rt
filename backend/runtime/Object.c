#include "Object.h"
#include "pool/pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdatomic.h>


/* int allocationCount[5]; */
/* pool globalPool1; */
/* pool globalPool2; */
/* pool globalPool3; */

/* Must be a multiple of processor word, 8 on 64 bit, 4 on 32 bit */

#define BLOCK_SIZE 8 * 40

void PersistentVector_initialise();
void Nil_initialise();

void initialise_memory() {
  /* memset(allocationCount, 0, sizeof(int)*5); */
  /* poolInitialize(&globalPool1, BLOCK_SIZE, 100000); */
  /* poolInitialize(&globalPool2, 128, 100000); */
  /* poolInitialize(&globalPool3, 64, 100000); */
  PersistentVector_initialise();
  Nil_initialise();
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
extern void deallocate(void *ptr);


extern void *data(Object *self);
extern Object *super(void *self);

extern void Object_create(Object *self, objectType type);
extern void retain(void *self);
extern BOOL release(void *self);
extern BOOL release_internal(void *self, BOOL deallocatesChildren);

extern void Object_retain(Object *self);
extern BOOL Object_release(Object *self);
extern BOOL Object_release_internal(Object *self, BOOL deallocatesChildren);


extern BOOL equals(Object *self, Object *other);
extern uint64_t hash(Object *self);
extern String *toString(Object *self);

