#include "Object.h"
#include "PersistentList.h"
#include "sds/sds.h"

static PersistentList *EMPTY = NULL;

PersistentList* PersistentList_empty() {
  if (EMPTY == NULL) EMPTY = PersistentList_create(NULL, NULL);
  return EMPTY;
} 

PersistentList* PersistentList_create(Object *first, PersistentList *rest) {
  Object *super = allocate(sizeof(PersistentList) + sizeof(Object)); 
  PersistentList *self = Object_data(super);

  self->first = first;
  self->rest = rest;
  self->count = (rest ? rest->count : 0) + (first ? 1 : 0);
  
  Object_create(super, persistentListType);
  return self;
}

BOOL PersistentList_equals(PersistentList *self, PersistentList *other) {
  if (self->count != other->count) return FALSE;

  PersistentList* selfPtr = self;
  PersistentList* otherPtr = other;

  while(selfPtr) {
    if (!(selfPtr->first == otherPtr->first || (selfPtr->first && otherPtr->first && Object_equals(selfPtr->first, otherPtr->first)))) return FALSE;
    selfPtr = selfPtr->rest;
    otherPtr = otherPtr->rest;
  }
  return TRUE;
}

uint64_t PersistentList_hash(PersistentList *self) {
  uint64_t h = 5381;
  h = ((h << 5) + h) + (self->first ? Object_hash(self->first) : 0);

  PersistentList *current = self->rest;
  while (current) {
    h = ((h << 5) + h) + hash(current);
  }
  return h;
}

String *PersistentList_toString(PersistentList *self) {
  String *retVal = String_create("(");
  String *space = String_create(" ");
  String *closing = String_create(")");

  PersistentList *current = self;

  while (current) {
    if (current != self && current->first) {
      retVal = String_append(retVal, space);
    }
    if(current->first) {
      String *s = Object_toString(current->first);
      retVal = String_append(retVal, s);
      release(s);
    }
    current = current->rest;
  }
  
  retVal = String_append(retVal, closing); 
  release(space);
  release(closing);
  return retVal;
}

void PersistentList_destroy(PersistentList *self, BOOL deallocateChildren) {
  if (deallocateChildren) {
    PersistentList *child = self->rest;
    while(child) {
      PersistentList *next = child->rest;
      if (!Object_release_internal(super(child), FALSE)) break;
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
