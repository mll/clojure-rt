#include "PersistentVector.h"
#include "Object.h"


PersistentVector* PersistentVector_create() {
  Object *super = allocate(sizeof(PersistentVector)+ sizeof(Object)); 
  PersistentVector *self = (PersistentVector *)(super + 1);
  self->count = 0;
  Object_create(super, persistentVectorType);
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







