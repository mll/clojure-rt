#ifndef RT_OBJECT
#define RT_OBJECT

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include <stdatomic.h>

typedef struct String String; 

enum objectType {
   integerType,
   stringType,
   persistentListType,
   persistentVectorType,
   persistentVectorNodeType
};

typedef enum objectType objectType;

struct Object {
  objectType type;
  volatile atomic_uint_fast64_t refCount;
};

typedef struct Object Object; 

void *allocate(size_t size);
void deallocate(void *ptr);
void initialise_memory();


void *data(Object *self);
Object *super(void *self);

void Object_create(Object *self, objectType type);
void retain(void *self);
bool release(void *self);
bool release_internal(void *self, bool deallocatesChildren);

void Object_retain(Object *self);
bool Object_release(Object *self);
bool Object_release_internal(Object *self, bool deallocatesChildren);


bool equals(Object *self, Object *other);
uint64_t hash(Object *self);
String *toString(Object *self);

#endif
