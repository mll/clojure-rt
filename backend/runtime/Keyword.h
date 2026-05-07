#ifndef RT_KEYWORD
#define RT_KEYWORD

#ifdef __cplusplus
extern "C" {
#endif

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
RTValue Keyword_invoke(RTValue self, RTValue map, RTValue defaultVal);
uint32_t Keyword_getInternCount();
void Keyword_resetInterns();

#ifdef __cplusplus
}
#endif

#endif
