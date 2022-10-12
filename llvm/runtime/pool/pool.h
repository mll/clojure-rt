#pragma once

#include <stdint.h>

#define POOL_BLOCKS_INITIAL 1

typedef struct poolFreed{
	struct poolFreed *nextFree;
} poolFreed;

typedef struct {
	uint64_t elementSize;
	uint64_t blockSize;
	uint64_t used;
	int64_t block;
	poolFreed *freed;
	uint64_t blocksUsed;
	uint8_t **blocks;
} pool;

void poolInitialize(pool *p, const uint64_t elementSize, const uint64_t blockSize);
void poolFreePool(pool *p);

#ifndef DISABLE_MEMORY_POOLING
void *poolMalloc(pool *p);
void poolFree(pool *p, void *ptr);
#else
#include <stdlib.h>
#define poolMalloc(p) malloc((p)->blockSize)
#define poolFree(p, d) free(d)
#endif
void poolFreeAll(pool *p);
