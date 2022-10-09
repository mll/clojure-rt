#include "PersistentVector.h"
#include "Object.h"


PersistentVector* PersistentVector_create() {
  PersistentVector *self = malloc(sizeof(PersistentVector));
  self->count = 0;
  
  self->super = Object_create(persistentVectorType, self);
  return self;
}

bool PersistentVector_equals(PersistentVector *self, PersistentVector *other) {
  return false;
}
uint64_t PersistentVector_hash(PersistentVector *self) {
  return 0;
}
String *PersistentVector_toString(PersistentVector *self) {
  return NULL;
}
void PersistentVector_destroy(PersistentVector *self, bool deallocateChildren) {

}

PersistentVector* PersistentVector_conj(PersistentVector *self, Object *other) {

return NULL;
}







