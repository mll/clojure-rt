#ifndef RT_RATIO
#define RT_RATIO

#ifdef __cplusplus
extern "C" {
#endif

#include "BigInteger.h"
#include "Double.h"
#include "RTValue.h"
#include "String.h"
#include <gmp.h>

typedef struct Object Object;

struct Ratio {
  Object super;
  mpq_t value;
};

typedef struct Ratio Ratio;

Ratio *Ratio_createUninitialized();
Ratio *Ratio_createUnassigned();
Ratio *Ratio_createFromStr(const char *value);
Ratio *Ratio_createFromMpq(mpq_t value);
Ratio *Ratio_createFromInts(int32_t numerator, int32_t denominator);
Ratio *Ratio_createFromInt(int32_t value);
Ratio *Ratio_createFromBigInteger(BigInteger *value);
RTValue Ratio_simplify(Ratio *value);
bool Ratio_equals(Ratio *self, Ratio *other);
uword_t Ratio_hash(Ratio *self);
String *Ratio_toString(Ratio *self);
void Ratio_destroy(Ratio *self);
double Ratio_toDouble(Ratio *self);

RTValue Ratio_add(Ratio *self, Ratio *other);
RTValue Ratio_sub(Ratio *self, Ratio *other);
RTValue Ratio_mul(Ratio *self, Ratio *other);
RTValue Ratio_div(Ratio *self, Ratio *other);
bool Ratio_gte(Ratio *self, Ratio *other);
bool Ratio_gt(Ratio *self, Ratio *other);
bool Ratio_lt(Ratio *self, Ratio *other);
bool Ratio_lte(Ratio *self, Ratio *other);

#ifdef __cplusplus
}
#endif

#endif
