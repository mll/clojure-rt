#ifndef RT_INTEGER
#define RT_INTEGER
#include "String.h"
#include "defines.h"


String *Integer_toString(int32_t self); 

void* Integer_div(int64_t num, int64_t den);

#endif
