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
#include "Interface.h"


typedef struct Object Object;
typedef struct Class Interface;

typedef struct ImplementedInterface {
  Interface *interface;
  ClojureFunction **functions;
} ImplementedInterface;

typedef struct Class {
  Object super;
  uword_t registerId;
  bool isInterface;

  String *name;
  String *className;
  
  struct Class *superclass;

  uword_t staticFieldCount;
  /* Keywords */
  RTValue *staticFieldNames;
  /* Anythings */
  RTValue *staticFields;

  uword_t staticMethodCount;
  /* Keywords */  
  RTValue *staticMethodNames;
  ClojureFunction **staticMethods;

  uword_t fieldCount;
  /* Keywords */
  RTValue *fieldNames;

  uword_t methodCount;
  /* Keywords */
  RTValue *methodNames;
  ClojureFunction **methods; 
  
  // This does not contain interfaces of the superclass
  uword_t implementedInterfacesCount;
  ImplementedInterface **implementedInterfaces;
} Class;

// Class owns all its arguments

Class *Class_create(bool isInterface, String *name,
                    String * className, struct Class * superclass,

                    uword_t staticFieldCount, RTValue * staticFieldNames,
                    RTValue * staticFields,

                    uword_t staticMethodCount, RTValue * staticMethodNames,
                    ClojureFunction * *staticMethods,

                    uword_t fieldCount, RTValue * fieldNames,

                    uword_t methodCount, RTValue * methodNames,
                    ClojureFunction * *methods,

                    uword_t implementedInterfacesCount,
                    ImplementedInterface * *implementedInterfaces);

bool Class_equals(Class *self, Class *other);
uword_t Class_hash(Class *self);
String *Class_toString(Class *self);
void Class_destroy(Class *self);

word_t Class_fieldIndex(Class *self, /* Keyword */ RTValue field);
word_t Class_staticFieldIndex(Class *self, /* Keyword */ RTValue staticField);
RTValue Class_setIndexedStaticField(Class *self, word_t i, RTValue value);
RTValue Class_getIndexedStaticField(Class *self, word_t i);
ClojureFunction *Class_resolveInstanceCall(Class *self, RTValue name, uword_t argCount);

#endif
