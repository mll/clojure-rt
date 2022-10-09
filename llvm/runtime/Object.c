#include "Object.h"
#include "String.h"
#include "Integer.h"
#include "PersistentList.h"
#include "PersistentVector.h"

Object* Object_create(objectType type, void *data) {
  Object *self = malloc(sizeof(Object));
  pthread_mutex_init(&(self->refCountLock), NULL);
  self->refCount = 1;
  self->type = type;
  self->data = data;
  return self;
}

bool equals(Object *self, Object *other) {
  if (self == other || self->data == other->data) return true;
  if (self->type != other->type) return false;
  switch((objectType)self->type) {
      case integerType:
        return Integer_equals(self->data, other->data);
        break;          
      case stringType:
        return String_equals(self->data, other->data);
        break;          
      case persistentListType:
        return PersistentList_equals(self->data, other->data);
        break;          
      case persistentVectorType:
        return PersistentVector_equals(self->data, other->data);
        break;
      }
}

uint64_t hash(Object *self) {
      switch((objectType)self->type) {
      case integerType:
        return Integer_hash(self->data);
        break;          
      case stringType:
        return String_hash(self->data);
        break;          
      case persistentListType:
        return PersistentList_hash(self->data);
        break;          
      case persistentVectorType:
        return PersistentVector_hash(self->data);
        break;
      }
}

String *toString(Object *self) {
    switch((objectType)self->type) {
      case integerType:
        return Integer_toString(self->data);
        break;          
      case stringType:
        return String_toString(self->data);
        break;          
      case persistentListType:
        return PersistentList_toString(self->data);
        break;          
      case persistentVectorType:
        return PersistentVector_toString(self->data);
        break;
      }
}

void retain(Object *self) {
  pthread_mutex_lock(&(self->refCountLock));
  self->refCount++;
  pthread_mutex_unlock(&(self->refCountLock));
}

bool release_internal(Object *self, bool deallocateChildren) {
    pthread_mutex_lock(&(self->refCountLock));
    if (--(self->refCount) == 0) {
      switch((objectType)self->type) {
      case integerType:
        Integer_destroy(self->data);
        break;          
      case stringType:
        String_destroy(self->data);
        break;          
      case persistentListType:
        PersistentList_destroy(self->data, deallocateChildren);
        break;          
      case persistentVectorType:
        PersistentVector_destroy(self->data, deallocateChildren);
        break;
      }
      free(self->data);
      pthread_mutex_unlock(&(self->refCountLock));
      pthread_mutex_destroy(&(self->refCountLock));
      free(self);
      return true;
    }
    pthread_mutex_unlock(&(self->refCountLock));
    return false;
}

bool release(Object *self) {
  return release_internal(self, true);
}

