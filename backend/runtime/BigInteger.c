#include "BigInteger.h"
#include "String.h"
#include "Object.h"

BigInteger* BigInteger_createUninitialized() {
  BigInteger *self = (BigInteger *)allocate(sizeof(BigInteger)); 
  Object_create((Object *)self, bigIntegerType);
  return self;
}

BigInteger* BigInteger_createUnassigned() {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init(self->value);
  return self;
}

/* mem done */
BigInteger* BigInteger_createFromStr(char * value) {
  BigInteger *self = BigInteger_createUninitialized();
  assert(mpz_init_set_str(self->value, value, 10) == 0 && "Failed to initialize BigInteger");
  return self;
}

/* mem done */
BigInteger* BigInteger_createFromMpz(mpz_t value) {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init_set(self->value, value);
  return self;
}

/* mem done */
BigInteger* BigInteger_createFromInt(int64_t value) {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init_set_si(self->value, value);
  return self;
}

/* outside refcount system */
BOOL BigInteger_equals(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  return cmp == 0;
}

/* outside refcount system */
uint64_t BigInteger_hash(BigInteger *self) {
  return combineHash(5381, mpz_get_ui(self->value) + 5381);
}

/* mem done */
String *BigInteger_toString(BigInteger *self) {  
  String *num = String_create(mpz_get_str(NULL, 10, self->value));
  String *suffix = String_create("N");
  String *retVal = String_concat(num, suffix);
  release(self);
  return retVal;
}

/* outside refcount system */
void BigInteger_destroy(BigInteger *self) {
  mpz_clear(self->value);
}

double BigInteger_toDouble(BigInteger *self) {
  double retVal = mpz_get_d(self->value);
  release(self);
  return retVal;
}

BigInteger *BigInteger_add(BigInteger *self, BigInteger *other) {
  BOOL selfReusable = isReusable(self);
  BOOL otherReusable = isReusable(other);
  BigInteger *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = BigInteger_createUnassigned();
  mpz_add(retVal->value, self->value, other->value);
  if (selfReusable) release(other);
  else if (otherReusable) release(self);
  else { release(self); release(other); }
  return retVal;
}

BigInteger *BigInteger_sub(BigInteger *self, BigInteger *other) {
  BOOL selfReusable = isReusable(self);
  BOOL otherReusable = isReusable(other);
  BigInteger *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = BigInteger_createUnassigned();
  mpz_sub(retVal->value, self->value, other->value);
  if (selfReusable) release(other);
  else if (otherReusable) release(self);
  else { release(self); release(other); }
  return retVal;
}

BigInteger *BigInteger_mul(BigInteger *self, BigInteger *other) {
  BOOL selfReusable = isReusable(self);
  BOOL otherReusable = isReusable(other);
  BigInteger *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = BigInteger_createUnassigned();
  mpz_mul(retVal->value, self->value, other->value);
  if (selfReusable) release(other);
  else if (otherReusable) release(self);
  else { release(self); release(other); }
  return retVal;
}

void *BigInteger_div(BigInteger *self, BigInteger *other) {
  if (!mpz_cmp_si(other->value, 0)) {
    release(self);
    release(other);
    return NULL; // Exception: divide by zero
  }
  
  if (mpz_divisible_p(self->value, other->value)) {
    BOOL selfReusable = isReusable(self);
    BOOL otherReusable = isReusable(other);
    BigInteger * retVal;
    if (selfReusable) retVal = self;
    else if (otherReusable) retVal = other;
    else retVal = BigInteger_createUnassigned();
    mpz_divexact(retVal->value, self->value, other->value);
    if (selfReusable) release(other);
    else if (otherReusable) release(self);
    else { release(self); release(other); }
    return retVal;
  } else {
    Ratio *ratio = Ratio_createUnassigned();
    mpq_set_num(ratio->value, self->value);
    mpq_set_den(ratio->value, other->value);
    mpq_canonicalize(ratio->value);
    release(self);
    release(other);
    return ratio;
  }
}

BOOL BigInteger_gte(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  release(self);
  release(other);
  return cmp >= 0;
}

BOOL BigInteger_lt(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  release(self);
  release(other);
  return cmp < 0;
}
