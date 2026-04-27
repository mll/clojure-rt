#include "BridgedObject.h"
#include "Ebr.h"
#include "RTValue.h"
#include "Class.h"
#include "Hash.h"
#include "Object.h"
#include "word.h"
#include <stdarg.h>
#include <stdatomic.h>

Class *ClassLookupByName(const char *className, void *jitEngine);
Class *ClassLookupByRegisterId(int32_t registerId, void *jitEngine);

Class *Class_createProtocol(String *name, String *className,
                             int32_t extendsProtocolCount,
                             Class **extendsProtocols) {
  Class *retVal =
      Class_create(name, className, extendsProtocolCount, extendsProtocols);
  retVal->isProtocol = true;
  return retVal;
}

Class *Class_create(String *name, String *className, int32_t superclassCount,
                    Class **superclasses) {

  Class *self = allocate(sizeof(Class));
  self->isProtocol = false;
  self->registerId = 0; // unregistered
  self->name = name;
  self->className = className;

  ClassList *list = allocate(sizeof(ClassList) + sizeof(Class *) * superclassCount);
  list->count = superclassCount;
  for (int32_t i = 0; i < superclassCount; i++) {
    list->classes[i] = superclasses[i];
    // superclasses[i] was already retained or owned by the caller's expectation
  }
  atomic_init(&self->superclasses, list);
  if (superclasses) {
    deallocate(superclasses);
  }

  atomic_init(&self->implementedProtocols, NULL);

  self->compilerExtension = NULL;
  self->compilerExtensionDestructor = NULL;
  Object_create((Object *)self, classType);
  return self;
}

/* outside refcount system */
bool Class_equals(Class *self, Class *other) {
  // Unregistered classes should never appear here
  return self->registerId == other->registerId;
}

/* outside refcount system */
int32_t
Class_hash(Class *self) { // CONSIDER: Ignoring fields for now, is it wise?
  return combineHash(avalanche(self->registerId),
                     Object_hash((Object *)self->name));
}

String *Class_toString(Class *self) {
  String *retVal = self->className;
  Ptr_retain(retVal);
  Ptr_release(self);
  return retVal;
}

void Class_destroy(Class *self) {
  Ptr_release(self->name);
  Ptr_release(self->className);
  ClassList *supers = atomic_load_explicit(&self->superclasses, memory_order_relaxed);
  if (supers) {
    for (int32_t i = 0; i < supers->count; i++)
      Ptr_release(supers->classes[i]);
    deallocate(supers);
  }

  ClassList *list = atomic_load_explicit(&self->implementedProtocols, memory_order_relaxed);
  if (list) {
    for (int32_t i = 0; i < list->count; i++) {
      Ptr_release(list->classes[i]);
    }
    deallocate(list);
  }

  if (self->compilerExtension && self->compilerExtensionDestructor) {
    self->compilerExtensionDestructor(self->compilerExtension);
  }
}

bool Class_isInstanceClassName(const char *className, void *jitEngine,
                               RTValue instance) {
  // Throws when class cannot be found.
  Class *cls = ClassLookupByName(className, jitEngine);
  assert(cls && "cls must not be null");
  Class *targetClass = ClassLookupByRegisterId(getType(instance), jitEngine);
  assert(targetClass && "targetClass must not be null");
  bool retVal = Class_isInstance(cls, targetClass);
  Ptr_release(cls);
  Ptr_release(targetClass);
  release(instance);
  return retVal;
}

/* outside refcount system */
/* TODO: This is not yet done for protocols */
bool Class_isInstance(Class *current, Class *target) {
  assert(target && "Target must not be null");
  assert(current && "Current must not be null");
  if (current->registerId == target->registerId || current->registerId == objectRootType) {
    return true;
  }
  ClassList *supers = atomic_load_explicit(&target->superclasses, memory_order_acquire);
  if (supers) {
    for (int32_t i = 0; i < supers->count; i++) {
      if (Class_isInstance(current, supers->classes[i])) {
        return true;
      }
    }
  }
  ClassList *list = atomic_load_explicit(&target->implementedProtocols, memory_order_acquire);
  if (list) {
    for (int32_t i = 0; i < list->count; i++) {
      if (Class_isInstance(current, list->classes[i])) {
        return true;
      }
    }
  }
  return false;
}

static void reclaim_class_list_destructor(void *contents, void *jit) {
  ClassList *list = (ClassList *)contents;
  for (int32_t i = 0; i < list->count; i++) {
    Ptr_release(list->classes[i]);
  }
  deallocate(list);
}

void Class_addProtocol(Class *self, Protocol *proto) {
  Ptr_retain(proto);
  while (true) {
    ClassList *oldList =
        atomic_load_explicit(&self->implementedProtocols, memory_order_acquire);

    int32_t oldCount = oldList ? oldList->count : 0;
    int32_t newCount = oldCount + 1;
    ClassList *newList =
        allocate(sizeof(ClassList) + sizeof(Class *) * newCount);
    newList->count = newCount;
    for (int32_t i = 0; i < oldCount; i++) {
      newList->classes[i] = oldList->classes[i];
      Ptr_retain(newList->classes[i]);
    }
    newList->classes[oldCount] = proto;
    // proto was retained at the start of function.

    if (atomic_compare_exchange_strong_explicit(
            &self->implementedProtocols, &oldList, newList,
            memory_order_release, memory_order_relaxed)) {
      if (oldList) {
        BridgedObject *bridge =
            BridgedObject_create(oldList, reclaim_class_list_destructor, NULL);
        autorelease(RT_boxPtr(bridge));
      }
      break;
    } else {
      // Failed CAS, cleanup newList and retry
      for (int32_t i = 0; i < newCount - 1; i++) {
        Ptr_release(newList->classes[i]);
      }
      // We keep proto retained for the next attempt
      deallocate(newList);
    }
  }
}
