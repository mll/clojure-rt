#ifndef RT_RATIO
#define RT_RATIO
#include "Double.h"
#include "String.h"
#include "BigInteger.h"
#include <gmp.h>
#include "RTValue.h"

typedef struct Object Object;

struct Ratio {
  Object super;
  mpq_t value;
};

typedef struct Ratio Ratio;

Ratio* Ratio_createUninitialized();
Ratio* Ratio_createUnassigned();
Ratio* Ratio_createFromStr(const char * value);
Ratio* Ratio_createFromMpq(mpq_t value);
Ratio* Ratio_createFromInts(word_t numerator, word_t denominator);
Ratio* Ratio_createFromInt(word_t value);
Ratio* Ratio_createFromBigInteger(BigInteger * value);
void* Ratio_simplify(Ratio * value);
bool Ratio_equals(Ratio *self, Ratio *other);
uword_t Ratio_hash(Ratio *self);
String *Ratio_toString(Ratio *self); 
void Ratio_destroy(Ratio *self);
double Ratio_toDouble(Ratio *self);

void *Ratio_add(Ratio *self, Ratio *other);
void *Ratio_sub(Ratio *self, Ratio *other);
void *Ratio_mul(Ratio *self, Ratio *other);
void *Ratio_div(Ratio *self, Ratio *other);
bool Ratio_gte(Ratio *self, Ratio *other);
bool Ratio_lt(Ratio *self, Ratio *other);

#endif
