#ifndef RT_BOOLEAN
#define RT_BOOLEAN

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"
#include "defines.h"

String *Boolean_toString(RTValue self);

#ifdef __cplusplus
}
#endif

#endif
