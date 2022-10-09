#ifndef RT_PERSISTENT_VECTOR
#define RT_PERSISTENT_VECTOR

#include "Object.h"

#define BRANCHING 32
#define BITSHIFT 5

typedef struct PersistentVector PersistentVector;
typedef struct PersistentVector_Node PersistentVector_Node;

struct PersistentVector_Node {
  Object* super;
  Object* array[BRANCHING]; 
};

struct PersistentVector {
  Object *super;
  PersistentVector_Node *root;
  Object *tail[BRANCHING];
  uint64_t count;
};

PersistentVector* PersistentVector_create();
bool PersistentVector_equals(PersistentVector *self, PersistentVector *other);
uint64_t PersistentVector_hash(PersistentVector *self);
String *PersistentVector_toString(PersistentVector *self);
void PersistentVector_destroy(PersistentVector *self, bool deallocateChildren);

PersistentVector* PersistentVector_conj(PersistentVector *self, Object *other);


#endif
