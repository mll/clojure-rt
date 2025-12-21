#ifndef RT_KEYWORD
#define RT_KEYWORD

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "ObjectProto.h"
#include "RTValue.h"
typedef struct String String; 

RTValue Keyword_create(String *string);
String *Keyword_toString(int32_t self);

#endif
