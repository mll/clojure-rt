#ifndef RT_SYMBOL
#define RT_SYMBOL

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "defines.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct String String;

RTValue Symbol_create(String *string);
String *Symbol_toString(RTValue self);
uint32_t Symbol_getInternCount();

#ifdef __cplusplus
}
#endif

#endif
