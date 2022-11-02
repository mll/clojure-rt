#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#include "String.h"

#define RRB_BITS 5
#define RRB_MAX_HEIGHT 7

#define RRB_BRANCHING (1 << RRB_BITS)
#define RRB_MASK (RRB_BRANCHING - 1)

typedef struct PersistentVector PersistentVector;
typedef struct PersistentVectorNode PersistentVectorNode;

struct PersistentVector {
  uint64_t count;
  uint64_t shift;
  PersistentVectorNode *tail;
  PersistentVectorNode *root;
};

PersistentVector* PersistentVector_create();

BOOL PersistentVector_equals(PersistentVector * restrict self, PersistentVector * restrict other);
uint64_t PersistentVector_hash(PersistentVector * restrict self);
String *PersistentVector_toString(PersistentVector * restrict self);
void PersistentVector_destroy(PersistentVector * restrict self, BOOL deallocateChildren);

PersistentVector* PersistentVector_conj(PersistentVector * restrict self, Object * restrict other);
PersistentVector* PersistentVector_assoc(PersistentVector * restrict self, uint64_t index, Object * restrict other);
Object* PersistentVector_nth(PersistentVector * restrict self, uint64_t index);
void PersistentVector_print(PersistentVector * restrict self);

#endif
