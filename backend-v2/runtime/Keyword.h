#ifndef RT_KEYWORD
#define RT_KEYWORD

#include "ObjectProto.h"
#include "RTValue.h"
#include "defines.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct String String;

RTValue Keyword_create(String *string);
String *Keyword_toString(RTValue self);
uint32_t Keyword_getInternCount();

#endif
