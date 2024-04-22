#ifndef RT_DEFTYPE
#define RT_DEFTYPE

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "PersistentArrayMap.h"
#include "Class.h"

struct Deftype {
  Class *_class;
  Object *values[]; // Alignment of values in array is equal to alignment of objects in class definition
};

typedef struct Deftype Deftype;

Deftype *Deftype_create(Class *_class, uint64_t fieldCount, ...);
BOOL Deftype_equals(Deftype *self, Deftype *other);
uint64_t Deftype_hash(Deftype *self);
String *Deftype_toString(Deftype *self);
void Deftype_destroy(Deftype *self);

Class *Deftype_getClass(Deftype *self);
void *Deftype_getField(Deftype *self, Keyword *field);
void *Deftype_getIndexedField(Deftype *self, int64_t i);

#endif
