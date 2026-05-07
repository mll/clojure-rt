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
RTValue Numbers_sin(RTValue a);
RTValue Numbers_cos(RTValue a);
RTValue Numbers_tan(RTValue a);
RTValue Numbers_asin(RTValue a);
RTValue Numbers_acos(RTValue a);
RTValue Numbers_atan(RTValue a);
RTValue Numbers_exp(RTValue a);
RTValue Numbers_exp2(RTValue a);
RTValue Numbers_exp10(RTValue a);
RTValue Numbers_log(RTValue a);
RTValue Numbers_log10(RTValue a);
RTValue Numbers_logb(RTValue a);
RTValue Numbers_log2(RTValue a);
RTValue Numbers_sqrt(RTValue a);
RTValue Numbers_cbrt(RTValue a);
RTValue Numbers_exp1m(RTValue a);
RTValue Numbers_log1p(RTValue a);
RTValue Numbers_abs(RTValue a);
RTValue Numbers_atan2(RTValue a, RTValue b);
RTValue Numbers_pow(RTValue a, RTValue b);
RTValue Numbers_hypot(RTValue a, RTValue b);

bool Numbers_lt(RTValue a, RTValue b);
bool Numbers_lte(RTValue a, RTValue b);
bool Numbers_gt(RTValue a, RTValue b);
bool Numbers_gte(RTValue a, RTValue b);
bool Numbers_equiv(RTValue a, RTValue b);

#ifdef __cplusplus
}
#endif

#endif
