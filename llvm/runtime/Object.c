#include "Object.h"
#include "String.h"
#include "Integer.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "wof_alloc/wof_allocator.h"

wof_allocator_t *alloc = NULL;

void initialise_memory() {
  alloc = wof_allocator_new();
}


void *allocate(size_t size) {
  return malloc(size);  //wof_alloc(alloc, size);
}

void deallocate(void *ptr) {
  free(ptr); // wof_free(alloc, ptr);
}

void *data(Object *self) {
  return self + 1;
}

Object *super(void *self) {
  return (Object *)(((Object *)self) - 1);
}


void Object_create(Object *self, objectType type) {
  atomic_init(&(self->refCount), 1);
  self->type = type;
}

bool equals(Object *self, Object *other) {
  void *selfData = data(self);
  void *otherData = data(other);
  if (self == other || selfData == otherData) return true;
  if (self->type != other->type) return false;
  switch((objectType)self->type) {
      case integerType:
        return Integer_equals(selfData, otherData);
        break;          
      case stringType:
        return String_equals(selfData, otherData);
        break;          
      case persistentListType:
        return PersistentList_equals(selfData, otherData);
        break;          
      case persistentVectorType:
        return PersistentVector_equals(selfData, otherData);
        break;
      }
}

uint64_t hash(Object *self) {
      switch((objectType)self->type) {
      case integerType:
        return Integer_hash(data(self));
        break;          
      case stringType:
        return String_hash(data(self));
        break;          
      case persistentListType:
        return PersistentList_hash(data(self));
        break;          
      case persistentVectorType:
        return PersistentVector_hash(data(self));
        break;
      }
}

String *toString(Object *self) {
    switch((objectType)self->type) {
      case integerType:
        return Integer_toString(data(self));
        break;          
      case stringType:
        return String_toString(data(self));
        break;          
      case persistentListType:
        return PersistentList_toString(data(self));
        break;          
      case persistentVectorType:
        return PersistentVector_toString(data(self));
        break;
      }
}

void retain(void *self) {
  Object_retain(super(self));
}

bool release(void *self) {
  return Object_release(super(self));
}

void Object_retain(Object *self) {
  atomic_fetch_add(&(self->refCount), 1);
}

bool Object_release_internal(Object *self, bool deallocateChildren) {
    
    if (atomic_fetch_sub(&(self->refCount), 1) == 1) {
      switch((objectType)self->type) {
      case integerType:
        Integer_destroy(data(self));
        break;          
      case stringType:
        String_destroy(data(self));
        break;          
      case persistentListType:
        PersistentList_destroy(data(self), deallocateChildren);
        break;          
      case persistentVectorType:
        PersistentVector_destroy(data(self), deallocateChildren);
        break;
      }
      deallocate(self);
      return true;
    }
    return false;
}

bool Object_release(Object *self) {
  return Object_release_internal(self, true);
}

