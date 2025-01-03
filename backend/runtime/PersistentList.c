#include "Object.h"
#include "PersistentList.h"

static PersistentList *EMPTY = NULL;

/* mem done */
PersistentList* PersistentList_empty() {
  if (EMPTY == NULL) EMPTY = PersistentList_create(NULL, NULL);
  retain(EMPTY);
  return EMPTY;
} 

/* outside refcount system */
PersistentList* PersistentList_create(void *first, PersistentList *rest) {
  PersistentList *self = allocate(sizeof(PersistentList)); 

  self->first = first;
  self->rest = rest;
  self->count = (rest ? rest->count : 0) + (first ? 1 : 0);
  
  Object_create((Object *)self, persistentListType);
  return self;
}

/* outside refcount system */
BOOL PersistentList_equals(PersistentList *self, PersistentList *other) {
  if (self->count != other->count) return FALSE;

  PersistentList* selfPtr = self;
  PersistentList* otherPtr = other;

  while(selfPtr) {
    if (!(selfPtr->first == otherPtr->first || (selfPtr->first && otherPtr->first && equals(selfPtr->first, otherPtr->first)))) return FALSE;
    selfPtr = selfPtr->rest;
    otherPtr = otherPtr->rest;
  }
  return TRUE;
}

/* outside refcount system */
uint64_t PersistentList_hash(PersistentList *self) {
  uint64_t h = 5381;
  h = combineHash(h, self->first ? hash(self->first) : 5381);

  PersistentList *current = self->rest;
  while (current) {
    h = combineHash(h, PersistentList_hash(current));
    current = self->rest;
  }
  return h;
}

/* mem done */
String *PersistentList_toString(PersistentList *self) {
  String *retVal = String_create("(");
  String *space = String_create(" ");
  String *closing = String_create(")");

  PersistentList *current = self;

  while (current) {
    if (current != self && current->first) {
      retain(space);
      retVal = String_concat(retVal, space);
    }
    if(current->first) {
      retain(current->first);
      String *s = toString(current->first);
      retVal = String_concat(retVal, s);
    }
    current = current->rest;
  }
  retVal = String_concat(retVal, closing); 
  release(space);  
  release(self);
  return retVal;
}

/* outside refcount system */
void PersistentList_destroy(PersistentList *self, BOOL deallocateChildren) {
  if (deallocateChildren) {
    PersistentList *child = self->rest;
    while(child) {
      PersistentList *next = child->rest;
      if (!Object_release_internal((Object *)child, FALSE)) {
        break;
      }

      child = next;
    }
  }
  if(self->first) release(self->first);  
}

/* mem done */
PersistentList* PersistentList_conj(PersistentList *self, void *other) {
  return PersistentList_create(other, self);
}
