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


RTValue Keyword_create(String *string);
String *Keyword_toString(uint32_t self);

#endif
