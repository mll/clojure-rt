#ifndef RT_BIG_INTEGER
#define RT_BIG_INTEGER
#include "Double.h"
#include "String.h"
#include <gmp.h>
#include "RTValue.h"

struct BigInteger {
  Object super;
  mpz_t value;
};

typedef struct BigInteger BigInteger;

BigInteger* BigInteger_createUninitialized();
BigInteger* BigInteger_createUnassigned();
BigInteger* BigInteger_createFromStr(const char * value);
BigInteger* BigInteger_createFromMpz(mpz_t value);
BigInteger* BigInteger_createFromInt(word_t value);
bool BigInteger_equals(BigInteger *self, BigInteger *other);
uword_t BigInteger_hash(BigInteger *self);
String *BigInteger_toString(BigInteger *self); 
void BigInteger_destroy(BigInteger *self);
double BigInteger_toDouble(BigInteger *self);

BigInteger *BigInteger_add(BigInteger *self, BigInteger *other);
BigInteger *BigInteger_sub(BigInteger *self, BigInteger *other);
BigInteger *BigInteger_mul(BigInteger *self, BigInteger *other);
void *BigInteger_div(BigInteger *self, BigInteger *other);
bool BigInteger_gte(BigInteger *self, BigInteger *other);
bool BigInteger_lt(BigInteger *self, BigInteger *other);

#endif
