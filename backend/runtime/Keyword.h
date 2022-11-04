#ifndef RT_KEYWORD
#define RT_KEYWORD

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"

typedef struct Object Object; 
typedef struct String String; 

struct Keyword {
  String *string;
};

typedef struct Keyword Keyword; 

Keyword* Keyword_create(String *string);
BOOL Keyword_equals(Keyword *self, Keyword *other);
uint64_t Keyword_hash(Keyword *self);
String *Keyword_toString(Keyword *self);
void Keyword_destroy(Keyword *self);


#endif
