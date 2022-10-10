#include "PersistentList.h"
#include "Object.h"

static PersistentList *EMPTY = NULL;

PersistentList* PersistentList_empty() {
  if (EMPTY == NULL) EMPTY = PersistentList_create(NULL, NULL);
  return EMPTY;
} 

PersistentList* PersistentList_create(Object *first, PersistentList *rest) {
  Object *super = allocate(sizeof(PersistentList) + sizeof(Object)); 
  PersistentList *self = data(super);

  self->first = first;
  self->rest = rest;
  self->count = (rest ? rest->count : 0) + (first ? 1 : 0);
  
  Object_create(super, persistentListType);
  return self;
}

bool PersistentList_equals(PersistentList *self, PersistentList *other) {
  if (self->count != other->count) return false;

  PersistentList* selfPtr = self;
  PersistentList* otherPtr = other;

  while(selfPtr) {
    if (!(selfPtr->first == otherPtr->first || (selfPtr->first && otherPtr->first && equals(selfPtr->first, otherPtr->first)))) return false;
    selfPtr = selfPtr->rest;
    otherPtr = otherPtr->rest;
  }
  return true;
}

uint64_t PersistentList_hash(PersistentList *self) {
  uint64_t h = 5381;
  h = ((h << 5) + h) + (self->first ? hash(self->first) : 0);

  PersistentList *current = self->rest;
  while (current) {
    h = ((h << 5) + h) + hash(super(current));
  }
  return h;
}

String *PersistentList_toString(PersistentList *self) {
  sds retVal = sdsnew("(");

  PersistentList *current = self;

  while (current) {
    if (current != self && current->first) retVal = sdscat(retVal, " ");
    if(current->first) {
      String *s = toString(current->first);
      sdscat(retVal, s->value);
      release(s);
    }
    current = current->rest;
  }
  retVal = sdscat(retVal, ")");
  return String_create(retVal);
}

void PersistentList_destroy(PersistentList *self, bool deallocateChildren) {
  if (deallocateChildren) {
    PersistentList *child = self->rest;
    while(child) {
      PersistentList *next = child->rest;
      if (!Object_release_internal(super(child), false)) break;
      child = next;
    }
  }
  if(self->first) Object_release(self->first);  
}


PersistentList* PersistentList_conj(PersistentList *self, Object *other) {
  retain(self);
  Object_retain(other);
  return PersistentList_create(other, self);
}
