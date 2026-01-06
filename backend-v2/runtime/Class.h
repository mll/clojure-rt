#ifndef RT_CLASS
#define RT_CLASS

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "Function.h"
#include "word.h"

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

  int32_t staticFieldCount;
  /* Keywords */
  RTValue *staticFieldNames;
  /* Anythings */
  RTValue *staticFields;

  int32_t staticMethodCount;
  /* Keywords */  
  RTValue *staticMethodNames;
  ClojureFunction **staticMethods;

  int32_t fieldCount;
  /* Keywords */
  RTValue *fieldNames;

  int32_t methodCount;
  /* Keywords */
  RTValue *methodNames;
  ClojureFunction **methods; 
  
  // This does not contain interfaces of the superclass
  int32_t implementedInterfacesCount;
  ImplementedInterface **implementedInterfaces;
} Class;

// Class owns all its arguments

Class *Class_create(String *name, String *className, int32_t superclassCount,
                    Class **superclasses,

                    int32_t staticFieldCount, RTValue *staticFieldNames,
                    RTValue *staticFields,

                    int32_t staticMethodCount, RTValue *staticMethodNames,
                    ClojureFunction **staticMethods,

                    int32_t fieldCount, RTValue *fieldNames,
                    int32_t methodCount, RTValue *methodNames,
                    ClojureFunction **methods,

                    int32_t implementedInterfacesCount,
                    ImplementedInterface **implementedInterfaces);



Class *Class_createInterface(String *name, String *className,
                             int32_t extendsInterfaceCount,
                             Class **extendsInterfaces,
                             int32_t staticMethodCount,
                             RTValue *staticMethodNames,
                             ClojureFunction **staticMethods,
                             int32_t methodCount, RTValue *methodNames);

bool Class_equals(Class *self, Class *other);
int32_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

int32_t Class_fieldIndex(Class *self, /* Keyword */ RTValue field);
int32_t Class_staticFieldIndex(Class *self, /* Keyword */ RTValue staticField);
RTValue Class_setIndexedStaticField(Class *self, int32_t i, RTValue value);
RTValue Class_getIndexedStaticField(Class *self, int32_t i);
ClojureFunction *Class_resolveInstanceCall(Class *self, RTValue name, int32_t argCount);

#endif
