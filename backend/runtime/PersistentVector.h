#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#include "String.h"

#define RRB_BITS 5
#define RRB_MAX_HEIGHT 7

#define RRB_BRANCHING (1 << RRB_BITS)
#define RRB_MASK (RRB_BRANCHING - 1)

/* 
 * The persistent vector's update performance is generally between 5-8x slower than an ArrayList.
 * The transient vector's update performance is comparable to the ArrayList, usually less than 1.5 times slower.
 * Iteration is about 3 times slower, and lookups take no more than 2 times more time 
 * (unless the vector contains less than 1000 elements, in which case it is about just as fast).
 */


typedef struct PersistentVector PersistentVector;
typedef struct PersistentVectorNode PersistentVectorNode;

struct PersistentVector {
  uint64_t count;
  uint64_t shift;
  uint64_t transientID;
  PersistentVectorNode *tail;
  PersistentVectorNode *root;
};

PersistentVector* PersistentVector_create();
PersistentVector* PersistentVector_createMany(uint64_t objCount, ...);

BOOL PersistentVector_equals(PersistentVector * restrict self, PersistentVector * restrict other);
uint64_t PersistentVector_hash(PersistentVector * restrict self);
String *PersistentVector_toString(PersistentVector * restrict self);
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren);

PersistentVector* PersistentVector_conj_internal(PersistentVector * restrict self, void * restrict other);
PersistentVector* PersistentVector_conj(PersistentVector * restrict self, void * restrict other);
PersistentVector* PersistentVector_conj_BANG_(PersistentVector * restrict self, void * restrict other);

PersistentVector* PersistentVector_assoc_internal(PersistentVector * restrict self, uint64_t index, void * restrict other);
PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, void * restrict other);
PersistentVector* PersistentVector_assoc_BANG_(PersistentVector * restrict self, uint64_t index, void * restrict other);

void* PersistentVector_dynamic_nth(PersistentVector * restrict self, void *indexObject);
void* PersistentVector_nth(PersistentVector * restrict self, uint64_t index);

void PersistentVector_print(PersistentVector * restrict self);

PersistentVector* PersistentVector_copy_root(PersistentVector * restrict self, uint64_t transientID);
PersistentVector* PersistentVector_transient(PersistentVector * restrict self);
PersistentVector* PersistentVector_persistent_BANG_(PersistentVector * restrict self);

#endif
