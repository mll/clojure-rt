#ifndef RT_PERSISTENT_LIST
#define RT_PERSISTENT_LIST

#include "String.h"

typedef struct Object Object;
typedef struct PersistentList PersistentList;

struct PersistentList {
  Object super;
  void *first;
  PersistentList *rest;
  uint64_t count;
};

PersistentList* PersistentList_empty();

BOOL PersistentList_equals(PersistentList *self, PersistentList *other);
uint64_t PersistentList_hash(PersistentList *self);
String *PersistentList_toString(PersistentList *self);
void PersistentList_destroy(PersistentList *self, BOOL deallocateChildren);

PersistentList* PersistentList_create(void *first, PersistentList *rest);
PersistentList* PersistentList_conj(PersistentList *self, void *other);


#endif
