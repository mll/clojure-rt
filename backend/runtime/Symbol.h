#ifndef RT_SYMBOL
#define RT_SYMBOL

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"

typedef struct Object Object; 
typedef struct String String; 

struct Symbol {
  String *name;
  String *namespace;
};

typedef struct Symbol Symbol; 

/* Transfers ownership - symbol swallows those two strings - it does not retain but it will free */
Symbol* Symbol_create(String *name, String *namespace);
BOOL Symbol_equals(Symbol *self, Symbol *other);
uint64_t Symbol_hash(Symbol *self);
String *Symbol_toString(Symbol *self);
void Symbol_destroy(Symbol *self);


#endif
