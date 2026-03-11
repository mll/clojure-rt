#ifndef RT_INTEGER
#define RT_INTEGER

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"
#include "defines.h"

String *Integer_toString(RTValue self);

RTValue Integer_div(int32_t num, int32_t den);

#ifdef __cplusplus
}
#endif

#endif
