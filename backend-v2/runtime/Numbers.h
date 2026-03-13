#ifndef RT_NUMBERS_H
#define RT_NUMBERS_H

#include "RTValue.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

RTValue Numbers_add(RTValue a, RTValue b);
RTValue Numbers_sub(RTValue a, RTValue b);
RTValue Numbers_mul(RTValue a, RTValue b);
RTValue Numbers_div(RTValue a, RTValue b);
double Numbers_toDouble(RTValue v);

bool Numbers_lt(RTValue a, RTValue b);
bool Numbers_lte(RTValue a, RTValue b);
bool Numbers_gt(RTValue a, RTValue b);
bool Numbers_gte(RTValue a, RTValue b);
bool Numbers_equiv(RTValue a, RTValue b);

#ifdef __cplusplus
}
#endif

#endif
