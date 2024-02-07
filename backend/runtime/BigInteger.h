#ifndef RT_BIG_INTEGER
#define RT_BIG_INTEGER
#include "Double.h"
#include "String.h"
#include <gmp.h>

struct BigInteger {
  mpz_t value;
};

typedef struct BigInteger BigInteger;

BigInteger* BigInteger_createUninitialized();
BigInteger* BigInteger_createUnassigned();
BigInteger* BigInteger_createFromStr(char * value);
BigInteger* BigInteger_createFromInt(int64_t value);
BOOL BigInteger_equals(BigInteger *self, BigInteger *other);
uint64_t BigInteger_hash(BigInteger *self);
String *BigInteger_toString(BigInteger *self); 
void BigInteger_destroy(BigInteger *self);
double BigInteger_toDouble(BigInteger *self);

BigInteger *BigInteger_add(BigInteger *self, BigInteger *other);
BigInteger *BigInteger_sub(BigInteger *self, BigInteger *other);
BigInteger *BigInteger_mul(BigInteger *self, BigInteger *other);
BigInteger *BigInteger_div(BigInteger *self, BigInteger *other);

#endif
