#ifndef RT_HASH
#define RT_HASH

#include "word.h"

static inline uint32_t avalanche_32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

static inline uint32_t deavalanche_32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x7ed1b41d;
  h ^= (h ^ (h >> 13)) >> 13;
  h *= 0xa5cb9243;
  h ^= h >> 16;
  return h;
}

static inline uint64_t avalanche_64(uint64_t h) {
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccd;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53;
  h ^= h >> 33;
  return h;
}

static inline uint64_t deavalanche_64(uint64_t h) {
  h ^= h >> 33;
  h *= 0x9cb4b2f8129337db;
  h ^= h >> 33;
  h *= 0x4f74430c22a54005;
  h ^= h >> 33;
  return h;
}

static inline uword_t avalanche(uword_t h) {
  /* One bit is always reserved so that zero can never be a hash */
#if INTPTR_MAX == INT64_MAX
  return avalanche_64(h) | (1ULL << 63);
#else
  return avalanche_32(h) | (1U << 31);
#endif
}

#endif
