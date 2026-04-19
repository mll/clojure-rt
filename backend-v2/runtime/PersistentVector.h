#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#ifdef __cplusplus
extern "C" {
#endif

#include "String.h"

#define RRB_BITS 5
#define RRB_MAX_HEIGHT 7

#define RRB_BRANCHING (1 << RRB_BITS)
#define RRB_MASK (RRB_BRANCHING - 1)

void PersistentVector_initialise();
void PersistentVector_cleanup();

/*
 * The persistent vector's update performance is generally between 5-8x slower
 * than an ArrayList. The transient vector's update performance is comparable to
 * the ArrayList, usually less than 1.5 times slower. Iteration is about 3 times
 * slower, and lookups take no more than 2 times more time (unless the vector
 * contains less than 1000 elements, in which case it is about just as fast).
 */

typedef struct PersistentVector PersistentVector;
typedef struct PersistentVectorNode PersistentVectorNode;

struct PersistentVector {
  Object super;
  uword_t count;
  uword_t shift;
  uword_t transientID;
  PersistentVectorNode *tail;
  PersistentVectorNode *root;
};

PersistentVector *PersistentVector_create();
PersistentVector *PersistentVector_createMany(int32_t objCount, ...);

bool PersistentVector_equals(PersistentVector *restrict self,
                             PersistentVector *restrict other);
uword_t PersistentVector_hash(PersistentVector *restrict self);
String *PersistentVector_toString(PersistentVector *restrict self);
void PersistentVector_destroy(PersistentVector *restrict self,
                              bool deallocateChildren);
void PersistentVector_promoteToShared(PersistentVector *self, uword_t current);

PersistentVector *
PersistentVector_conj_internal(PersistentVector *restrict self, RTValue other);
PersistentVector *PersistentVector_conj(PersistentVector *restrict self,
                                        RTValue other);
PersistentVector *PersistentVector_conj_BANG_(PersistentVector *restrict self,
                                              RTValue other);

PersistentVector *
PersistentVector_assoc_internal(PersistentVector *restrict self, uword_t index,
                                RTValue other);
PersistentVector *PersistentVector_assoc(PersistentVector *restrict self,
                                         uword_t index, RTValue other);
PersistentVector *PersistentVector_assoc_BANG_(PersistentVector *restrict self,
                                               uword_t index, RTValue other);

PersistentVector *
PersistentVector_pop_internal(PersistentVector *restrict self);
PersistentVector *PersistentVector_pop(PersistentVector *restrict self);
PersistentVector *PersistentVector_pop_BANG_(PersistentVector *restrict self);

RTValue PersistentVector_dynamic_nth(PersistentVector *restrict self,
                                     RTValue indexObject);
RTValue PersistentVector_nth(PersistentVector *restrict self, uword_t index);
PersistentVectorNode *PersistentVector_nthBlock(PersistentVector *restrict self,
                                                uword_t index);

void PersistentVector_print(PersistentVector *restrict self);

PersistentVector *PersistentVector_copy_root(PersistentVector *restrict self,
                                             uword_t transientID);
PersistentVector *PersistentVector_transient(PersistentVector *restrict self);
PersistentVector *
PersistentVector_persistent_BANG_(PersistentVector *restrict self);

uword_t PersistentVector_count(PersistentVector *restrict self);
bool PersistentVector_contains(PersistentVector *restrict self, RTValue other);

#ifdef __cplusplus
}
#endif

#endif
