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

void PersistentList_initialise();
PersistentList *PersistentList_empty();
void PersistentList_cleanup();

bool PersistentList_equals(PersistentList *self, PersistentList *other);
uint64_t PersistentList_hash(PersistentList *self);
String *PersistentList_toString(PersistentList *self);
void PersistentList_destroy(PersistentList *self, bool deallocateChildren);
void PersistentList_promoteToShared(PersistentList *self, uword_t current);

PersistentList *PersistentList_create(RTValue first, PersistentList *rest);
PersistentList *PersistentList_conj(PersistentList *self, RTValue other);
PersistentList *PersistentList_createMany(int32_t argCount, ...);
PersistentList *PersistentList_fromArray(int32_t argCount, RTValue *args);
RTValue RT_createListFromArray(int32_t argCount, RTValue *args);

#ifdef __cplusplus
}
#endif

#endif
