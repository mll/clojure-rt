#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "pool.h"

#ifndef max
#define max(a,b) ((a)<(b)?(b):(a))
#endif

void poolInitialize(pool *p, const uint64_t elementSize, const uint64_t blockSize)
{
	uint64_t i;

	p->elementSize = max(elementSize, sizeof(poolFreed));
	p->blockSize = blockSize;
	
	poolFreeAll(p);

	p->blocksUsed = POOL_BLOCKS_INITIAL;
	p->blocks = malloc(sizeof(uint8_t*)* p->blocksUsed);
        assert(p->blocks);

	for(i = 0; i < p->blocksUsed; ++i)
		p->blocks[i] = NULL;
}

void poolFreePool(pool *p)
{
	uint64_t i;
	for(i = 0; i < p->blocksUsed; ++i) {
		if(p->blocks[i] == NULL)
			break;
		else
			free(p->blocks[i]);
	}

	free(p->blocks);
}

#ifndef DISABLE_MEMORY_POOLING
void *poolMalloc(pool *p)
{
  if(p->freed != NULL) {
    void *recycle = p->freed;
    p->freed = p->freed->nextFree;
    return recycle;
  }
  
  if(++p->used == p->blockSize) {
    p->used = 0;
    if(++p->block == (int64_t)p->blocksUsed) {
      uint64_t i;
      
      p->blocksUsed <<= 1;
      p->blocks = realloc(p->blocks, sizeof(uint8_t*)* p->blocksUsed);
      assert(p->blocks);
      
      for(i = p->blocksUsed >> 1; i < p->blocksUsed; ++i)
        p->blocks[i] = NULL;
    }
    
    if(p->blocks[p->block] == NULL) 
      p->blocks[p->block] = malloc(p->elementSize * p->blockSize);
    assert(p->blocks[p->block]);
  }
  
  return p->blocks[p->block] + p->used * p->elementSize;
}

void poolFree(pool *p, void *ptr)
{
	poolFreed *pFreed = p->freed;

	p->freed = ptr;
	p->freed->nextFree = pFreed;
}
#endif

void poolFreeAll(pool *p)
{
	p->used = p->blockSize - 1;
	p->block = -1;
	p->freed = NULL;
}
