#ifndef RT_CLASS
#define RT_CLASS

#ifdef __cplusplus
extern "C" {
#endif

#include "Function.h"
#include "defines.h"
#include "word.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Object Object;
typedef struct Class Interface;

typedef struct ImplementedInterface {
  Interface *interface;
  ClojureFunction **functions;
} ImplementedInterface;

typedef struct Class {
  Object super;
  int32_t registerId;
  bool isInterface;

  String *name;
  String *className;

  int32_t superclassCount;
  struct Class **superclasses;

  void *compilerExtension;
  void (*compilerExtensionDestructor)(void *);
} Class;

// Class owns all its arguments

Class *Class_create(String *name, String *className, int32_t superclassCount,
                    Class **superclasses);

Class *Class_createInterface(String *name, String *className,
                             int32_t extendsInterfaceCount,
                             Class **extendsInterfaces);

bool Class_equals(Class *self, Class *other);
int32_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

bool Class_isInstanceClassName(const char *className, void *jitEngine,
                               RTValue instance);
bool Class_isInstance(Class *current, Class *target);

#ifdef __cplusplus
}
#endif

#endif
