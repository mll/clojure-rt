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
#include "Function.h"

typedef struct Object Object;

typedef struct Class {
  Object super;
  uint64_t registerId;
  String *name;
  String *className;
  BOOL isInterface;
  struct Class *superclass;
  
  // Static fields: HashMap vs list?
  uint64_t staticFieldCount;
  Keyword **staticFieldNames;
  Object **staticFields;
  
  // Static methods are in both global map and here (for now, ultimately they should be only here? Or is it an optimization to keep them in global map?)
  uint64_t staticMethodCount;
  Keyword **staticMethodNames;
  ClojureFunction **staticMethods;
  
  uint64_t fieldCount;
  uint64_t *indexPermutation; // !!! Index permutation vs fields inheritance
  Keyword **fieldNames;
  
  // Names of methods in the definition of the class (also in case of interface)
  uint64_t methodCount;
  Keyword **methodNames;
  ClojureFunction **methods; // TODO: If the class is an interface, there should be a dummy implementation, but what does it do?
  
  // This does not contain interfaces of the superclass
  uint64_t implementedInterfacesCount;
  struct Class **implementedInterfaceClasses;
  // 2d array, implementedInterfaces has length implementedInterfacesCount,
  // implementedInterfacesCount[i] has length implementedInterfaces[i]->methodCount
  ClojureFunction ***implementedInterfaces;
} Class;

Class* Class_create(
  String *name,
  String *className,
  uint64_t flags,
  
  Class *superclass,

  uint64_t staticFieldCount,
  Keyword **staticFieldNames, // pass ownership of pointer, array of length staticFieldCount
  Object **staticFields, // pass ownership of pointer, array of length staticFieldCount
  
  uint64_t staticMethodCount,
  Keyword **staticMethodNames, // pass ownership of pointer, array of length staticMethodCount
  ClojureFunction **staticMethods, // pass ownership of pointer, array of length staticMethodCount
  
  uint64_t fieldCount,
  Keyword **fieldNames, // function will free that pointer and create new to hold reordered fields
  
  uint64_t methodCount,
  Keyword **methodNames, // pass ownership of pointer, array of length methodCount
  ClojureFunction **methods, // pass ownership of pointer, array of length methodCount
  
  uint64_t implementedInterfacesCount,
  Class **implementedInterfaces, // pass ownership of pointer, array of length implementedInterfacesCount
  // pass ownership of pointer, 2d array of length implementedInterfacesCount,
  // implementedInterfacesCount[i] has length implementedInterfaces[i]->methodCount
  ClojureFunction ***implementedInterfacesMethods
); 
BOOL Class_equals(Class *self, Class *other);
uint64_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

int64_t Class_fieldIndex(Class *self, Keyword *field);
int64_t Class_staticFieldIndex(Class *self, Keyword *staticField);
void *Class_setIndexedStaticField(Class *self, int64_t i, void *value);
void *Class_getIndexedStaticField(Class *self, int64_t i);
ClojureFunction *Class_resolveInstanceCall(Class *self, Keyword *name, uint64_t argCount);

#endif
