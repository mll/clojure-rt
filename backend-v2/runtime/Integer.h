#ifndef RT_INTEGER
#define RT_INTEGER
#include "RTValue.h"
#include "String.h"
#include "defines.h"

String *Integer_toString(RTValue self); 

RTValue Integer_div(int32_t num, int32_t den);

#endif
