#ifndef RT_PERSISTENT_LIST
#define RT_PERSISTENT_LIST

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"

typedef struct Object Object;
typedef struct PersistentList PersistentList;

struct PersistentList {
  Object super;
  RTValue first;
  PersistentList *rest;
  uint64_t count;
};

PersistentList *PersistentList_empty();

bool PersistentList_equals(PersistentList *self, PersistentList *other);
uint64_t PersistentList_hash(PersistentList *self);
String *PersistentList_toString(PersistentList *self);
void PersistentList_destroy(PersistentList *self, bool deallocateChildren);

PersistentList *PersistentList_create(RTValue first, PersistentList *rest);
PersistentList *PersistentList_conj(PersistentList *self, RTValue other);
PersistentList *PersistentList_createMany(int32_t argCount, ...);

#ifdef __cplusplus
}
#endif

#endif
