#include "Object.h"
#include "PersistentList.h"
#include "RTValue.h"
#include <stdarg.h>


static PersistentList *EMPTY = NULL;
// How to mark 
/* mem done */
PersistentList* PersistentList_empty() {
  if (EMPTY == NULL) EMPTY = PersistentList_create(RT_boxNull(), NULL);
  Ptr_retain(EMPTY);
  return EMPTY;
}

struct ListPair {
  PersistentList *first;
  PersistentList *last;
};


/* outside refcount system, mutable */
PersistentList *reverse(PersistentList *self) {
  if (self == NULL) return NULL;

  PersistentList *prev = NULL;
  PersistentList *current = self;
  PersistentList *next = NULL;
  
  while (current != NULL) {
    next = current->rest; 
    current->rest = prev;
    prev = current;
    
    current = next;       
  }

  PersistentList *temp = prev;
  uword_t runningCount = 0;
    
  while (temp != NULL) {
    temp->count = runningCount;
    runningCount++;
    temp = temp->rest;
  }
  
  return prev;
}

PersistentList *PersistentList_createMany(uword_t argCount, ...) {
  PersistentList *retVal = PersistentList_empty();

  va_list args;  
  va_start(args, argCount);  

  while (argCount > 0) {
    retVal = PersistentList_conj(retVal, va_arg(args, RTValue));
    argCount--;
  }
  
  va_end(args);  
  return reverse(retVal);
}  


/* outside refcount system */
PersistentList* PersistentList_create(RTValue first, PersistentList *rest) {
  PersistentList *self = allocate(sizeof(PersistentList)); 

  self->first = first;
  self->rest = rest;
  self->count = (rest ? rest->count : 0) + (RT_isNull(first) ? 0 : 1);
  
  Object_create((Object *)self, persistentListType);
  return self;
}

/* outside refcount system */
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

/* outside refcount system */
uint64_t PersistentList_hash(PersistentList *self) {
  uint64_t h = 5381;
  h = combineHash(h, !RT_isNull(self->first) ? hash(self->first) : 5381);

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
    if (current != self && !RT_isNull(current->first)) {
      Ptr_retain(space);
      retVal = String_concat(retVal, space);
    }
    if(!RT_isNull(current->first)) {
      retain(current->first);
      String *s = toString(current->first);
      retVal = String_concat(retVal, s);
    }
    current = current->rest;
  }
  retVal = String_concat(retVal, closing); 
  Ptr_release(space);  
  Ptr_release(self);
  return retVal;
}

/* outside refcount system */
void PersistentList_destroy(PersistentList *self, bool deallocateChildren) {
  if (deallocateChildren) {
    PersistentList *child = self->rest;
    while(child) {
      PersistentList *next = child->rest;
      if (!Object_release_internal((Object *)child, false)) {
        break;
      }

      child = next;
    }
  }
  if(!RT_isNull(self->first)) release(self->first);  
}

/* mem done */
PersistentList* PersistentList_conj(PersistentList *self, RTValue other) {
  return PersistentList_create(other, self);
}
