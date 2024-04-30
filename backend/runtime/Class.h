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
  
  // Static fields: HashMap vs list?
  uint64_t staticFieldCount;
  Keyword **staticFieldNames;
  Object **staticFields;
  
  // Static methods are in global map
  
  uint64_t fieldCount;
  uint64_t *indexPermutation;
  Keyword *fields[];
};

typedef struct Class Class;

Class* Class_create(String *name, String *className,
                    uint64_t staticFieldCount,
                    Keyword **staticFieldNames, // pass ownership of pointer, array of length staticFieldCount
                    Object **staticFields, // pass ownership of pointer, array of length staticFieldCount
                    uint64_t fieldCount, ...); // Keyword*
BOOL Class_equals(Class *self, Class *other);
uint64_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

int64_t Class_fieldIndex(Class *self, Keyword *field);
int64_t Class_staticFieldIndex(Class *self, Keyword *staticField);
void *Class_setIndexedStaticField(Class *self, int64_t i, void *value);
void *Class_getIndexedStaticField(Class *self, int64_t i);

#endif
