#include "BigInteger.h"
#include "String.h"
#include "Object.h"

BigInteger* BigInteger_createUninitialized() {
  Object *super = allocate(sizeof(BigInteger) + sizeof(Object)); 
  BigInteger *self = (BigInteger *)(super + 1);
  Object_create(super, bigIntegerType);
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
BigInteger* BigInteger_createFromInt(int64_t value) {
  BigInteger *self = BigInteger_createUninitialized();
  mpz_init_set_si(self->value, value);
  return self;
}

/* outside refcount system */
BOOL BigInteger_equals(BigInteger *self, BigInteger *other) {
  return (mpz_cmp(self->value, other->value) == 0);
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
  BigInteger *retVal = BigInteger_createUnassigned();
  mpz_add(retVal->value, self->value, other->value);
  release(self);
  release(other);
  return retVal;
}

BigInteger *BigInteger_sub(BigInteger *self, BigInteger *other) {
  BigInteger *retVal = BigInteger_createUnassigned();
  mpz_sub(retVal->value, self->value, other->value);
  release(self);
  release(other);
  return retVal;
}

BigInteger *BigInteger_mul(BigInteger *self, BigInteger *other) {
  BigInteger *retVal = BigInteger_createUnassigned();
  mpz_mul(retVal->value, self->value, other->value);
  release(self);
  release(other);
  return retVal;
}

BigInteger *BigInteger_div(BigInteger *self, BigInteger *other) {
  // TODO: zero checks, creating ratio out of division of two integers
  BigInteger *retVal = BigInteger_createUnassigned();
  mpz_cdiv_q(retVal->value, self->value, other->value);
  release(self);
  release(other);
  return retVal;
}
