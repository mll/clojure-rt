#ifndef RT_CLASS
#define RT_CLASS

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "Keyword.h"
#include "PersistentArrayMap.h"

struct Class {
  uint64_t registerId;
  String *name;
  String *className;
  
  uint64_t fieldCount;
  String *fields[];
};

typedef struct Class Class;

Class* Class_create(String *name, String *className, uint64_t fieldCount, ...); // String*
BOOL Class_equals(Class *self, Class *other);
uint64_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

int64_t Class_fieldIndex(Class *self, String *field);

#endif
