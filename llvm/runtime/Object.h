#ifndef RT_OBJECT
#define RT_OBJECT

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"

typedef struct String String; 

enum objectType {
   integerType,
   stringType,
   persistentListType,
   persistentVectorType
};

typedef enum objectType objectType;

struct Object {
  small_enum type;
  pthread_mutex_t refCountLock;
  uint64_t refCount;
  void *data;
};

typedef struct Object Object; 

Object* Object_create(objectType type, void* data);
void retain(Object *self);
bool release(Object *self);
bool release_internal(Object *self, bool deallocatesChildren);


bool equals(Object *self, Object *other);
uint64_t hash(Object *self);
String *toString(Object *self);

#endif
