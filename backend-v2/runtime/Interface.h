#ifndef RT_INTERFACE
#define RT_INTERFACE

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "Function.h"
#include "word.h"


typedef struct Object Object;

typedef struct Interface {
  Object super;
  uword_t registerId;
  String *name;
  String *className;

  uword_t methodCount;
  /* Keywords */
  RTValue *methodNames;
  ClojureFunction **methods; 
} Interface;

// Interface owns all its arguments

Interface *Interface_create(String *name, String * className,
                            uword_t methodCount,
                            RTValue * methodNames, ClojureFunction * *methods);

bool Interface_equals(Interface *self, Interface *other);
uword_t Interface_hash(Interface *self);
String *Interface_toString(Interface *self);
void Interface_destroy(Interface *self);

#endif
