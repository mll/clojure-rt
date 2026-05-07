#include "BigInteger.h"
#include "Exceptions.h"
#include "Object.h"
#include "String.h"

BigInteger *BigInteger_createUninitialized() {
  BigInteger *self = (BigInteger *)allocate(sizeof(BigInteger));
  Object_create((Object *)self, bigIntegerType);
  return self;
}

BigInteger *BigInteger_createUnassigned() {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init(self->value);
  return self;
}

/* mem done */
BigInteger *BigInteger_createFromStr(const char *value) {
  BigInteger *self = BigInteger_createUninitialized();
  assert(mpz_init_set_str(self->value, value, 10) == 0 &&
         "Failed to initialize BigInteger");
  return self;
}

/* mem done */
BigInteger *BigInteger_createFromMpz(mpz_t value) {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init_set(self->value, value);
  return self;
}

/* mem done */
BigInteger *BigInteger_createFromInt(int32_t value) {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init_set_si(self->value, value);
  return self;
}

/* outside refcount system */
bool BigInteger_equals(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  return cmp == 0;
}

bool BigInteger_intEquals(int32_t other, BigInteger *self) {
  int cmp = mpz_cmp_si(self->value, other);
  Ptr_release(self);
  return cmp == 0;
}

bool BigInteger_equalsInt(BigInteger *self, int32_t other) {
  int cmp = mpz_cmp_si(self->value, other);
  Ptr_release(self);
  return cmp == 0;
}

bool BigInteger_equiv(BigInteger *self, BigInteger *other) {
  bool retVal = BigInteger_equals(self, other);
  Ptr_release(self);
  Ptr_release(other);
  return retVal;
}

/* outside refcount system */
uword_t BigInteger_hash(BigInteger *self) {
  return combineHash(5381, mpz_get_ui(self->value) + 5381);
}

/* mem done */
String *BigInteger_toString(BigInteger *self) {
  char *str = mpz_get_str(NULL, 10, self->value);
  String *num = String_createDynamicStr(str);
  free(str);
  String *suffix = String_create("N");
  String *retVal = String_concat(num, suffix);
  Ptr_release(self);
  return retVal;
}

/* outside refcount system */
void BigInteger_destroy(BigInteger *self) { mpz_clear(self->value); }

double BigInteger_toDouble(BigInteger *self) {
  double retVal = mpz_get_d(self->value);
  Ptr_release(self);
  return retVal;
}

BigInteger *BigInteger_add(BigInteger *self, BigInteger *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  BigInteger *retVal;
  if (selfReusable)
    retVal = self;
  else if (otherReusable)
    retVal = other;
  else
    retVal = BigInteger_createUnassigned();
  mpz_add(retVal->value, self->value, other->value);
  if (selfReusable)
    Ptr_release(other);
  else if (otherReusable)
    Ptr_release(self);
  else {
    Ptr_release(self);
    Ptr_release(other);
  }
  return retVal;
}

BigInteger *BigInteger_sub(BigInteger *self, BigInteger *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  BigInteger *retVal;
  if (selfReusable)
    retVal = self;
  else if (otherReusable)
    retVal = other;
  else
    retVal = BigInteger_createUnassigned();
  mpz_sub(retVal->value, self->value, other->value);
  if (selfReusable)
    Ptr_release(other);
  else if (otherReusable)
    Ptr_release(self);
  else {
    Ptr_release(self);
    Ptr_release(other);
  }
  return retVal;
}

BigInteger *BigInteger_mul(BigInteger *self, BigInteger *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  BigInteger *retVal;
  if (selfReusable)
    retVal = self;
  else if (otherReusable)
    retVal = other;
  else
    retVal = BigInteger_createUnassigned();
  mpz_mul(retVal->value, self->value, other->value);
  if (selfReusable)
    Ptr_release(other);
  else if (otherReusable)
    Ptr_release(self);
  else {
    Ptr_release(self);
    Ptr_release(other);
  }
  return retVal;
}

RTValue BigInteger_div(BigInteger *self, BigInteger *other) {
  if (!mpz_cmp_si(other->value, 0)) {
    Ptr_release(self);
    Ptr_release(other);
    throwArithmeticException_C("Divide by zero");
  }

  if (mpz_divisible_p(self->value, other->value)) {
    bool selfReusable = Ptr_isReusable(self);
    bool otherReusable = Ptr_isReusable(other);
    BigInteger *retVal;
    if (selfReusable)
      retVal = self;
    else if (otherReusable)
      retVal = other;
    else
      retVal = BigInteger_createUnassigned();
    mpz_divexact(retVal->value, self->value, other->value);
    if (selfReusable)
      Ptr_release(other);
    else if (otherReusable)
      Ptr_release(self);
    else {
      Ptr_release(self);
      Ptr_release(other);
    }
    return RT_boxPtr(retVal);
  } else {
    Ratio *ratio = Ratio_createUnassigned();
    mpq_set_num(ratio->value, self->value);
    mpq_set_den(ratio->value, other->value);
    mpq_canonicalize(ratio->value);
    Ptr_release(self);
    Ptr_release(other);
    return RT_boxPtr(ratio);
  }
}

bool BigInteger_gte(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp >= 0;
}

bool BigInteger_gt(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp > 0;
}

bool BigInteger_lt(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp < 0;
}

bool BigInteger_lte(BigInteger *self, BigInteger *other) {
  int cmp = mpz_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp <= 0;
}
