#ifndef RT_STRING
#define RT_STRING

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "sds/sds.h"
#include "defines.h"

typedef struct Object Object; 

struct String {
  Object *super;
  sds value;
  uint count;
};

typedef struct String String; 

String* String_create(sds string);
bool String_equals(String *self, String *other);
uint64_t String_hash(String *self);
String *String_toString(String *self);
void String_destroy(String *self);

#endif
