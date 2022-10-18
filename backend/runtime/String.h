#ifndef RT_STRING
#define RT_STRING

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"

typedef struct Object Object; 

struct String {
  char *value;
  unsigned int count;
};

typedef struct String String; 

String* String_create(char *string);
BOOL String_equals(String *self, String *other);
uint64_t String_hash(String *self);
String *String_toString(String *self);
void String_destroy(String *self);




#endif
