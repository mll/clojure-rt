#ifndef RT_SYMBOL
#define RT_SYMBOL

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "RTValue.h"

RTValue Symbol_create(String *name);
String *Symbol_toString(uint32_t self);

#endif
