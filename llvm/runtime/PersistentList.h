#ifndef RT_PERSISTENT_LIST
#define RT_PERSISTENT_LIST

#include "Object.h"
#include "String.h"

typedef struct Object Object;
typedef struct PersistentList PersistentList;

struct PersistentList {
  Object *super;
  Object *first;
  PersistentList *rest;
  uint64_t count;
};

PersistentList* PersistentList_empty();
PersistentList* PersistentList_create(Object *first, PersistentList *rest);
bool PersistentList_equals(PersistentList *self, PersistentList *other);
uint64_t PersistentList_hash(PersistentList *self);
String *PersistentList_toString(PersistentList *self);
void PersistentList_destroy(PersistentList *self, bool deallocateChildren);

PersistentList* PersistentList_conj(PersistentList *self, Object *other);


#endif
