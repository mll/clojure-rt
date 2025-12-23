#include "Ratio.h"
#include "String.h"
#include "Object.h"

Ratio* Ratio_createUninitialized() {
  Ratio *self = (Ratio *)allocate(sizeof(Ratio)); 
  Object_create((Object *)self, ratioType);
  return self;
}

Ratio* Ratio_createUnassigned() {
  Ratio *self = Ratio_createUninitialized();
  mpq_init(self->value);
  return self;
}

/* mem done */
Ratio* Ratio_createFromStr(char * value) {
  Ratio *self = Ratio_createUnassigned();
  assert(mpq_set_str(self->value, value, 10) == 0 && "Failed to initialize Ratio");
  mpq_canonicalize(self->value);
  return self;
}

/* mem done */
Ratio* Ratio_createFromMpq(mpq_t value) {
  Ratio *self = Ratio_createUnassigned();
  mpq_set(self->value, value);
  return self;
}

/* mem done */
Ratio* Ratio_createFromInts(word_t numerator, word_t denominator) {
  if (!denominator) return NULL; // Exception: divide by zero
  Ratio *self = Ratio_createUnassigned();
  mpq_set_si(self->value, numerator, denominator);
  mpq_canonicalize(self->value);
  return self;
}

/* mem done */
Ratio* Ratio_createFromInt(word_t value) {
  Ratio *self = Ratio_createUnassigned();
  mpq_set_si(self->value, value, 1);
  // canonicalized
  return self;
}

/* mem done */
Ratio* Ratio_createFromBigInteger(BigInteger * value) {
  Ratio *self = Ratio_createUnassigned();
  mpq_set_z(self->value, value->value);
  Ptr_release(value);
  // canonicalized
  return self;
}

bool mpq_isInteger(Ratio * ratio) {
  return mpz_cmp_si(mpq_denref(ratio->value), 1) == 0;
}

/* mem done */
void* Ratio_simplify(Ratio * self) {
  // If Ratio is integer, create BigInteger from Ratio, otherwise return self
  if (mpq_isInteger(self)) {
    BigInteger *num = BigInteger_createUnassigned();
    mpq_get_num(num->value, self->value);
    Ptr_release(self);
    return num;
  } else {
    return self;
  }
}

/* outside refcount system */
bool Ratio_equals(Ratio *self, Ratio *other) {
  int cmp = mpq_equal(self->value, other->value);
  return cmp == 0;
}

/* outside refcount system */
uword_t Ratio_hash(Ratio *self) {
  return combineHash(combineHash(5381, mpz_get_si(mpq_numref(self->value))), mpz_get_si(mpq_denref(self->value)));
}

/* mem done */
String *Ratio_toString(Ratio *self) {
  // Do not use BigInteger_toString! Numerator and denominator do not display N
  String *num = String_create(mpq_get_str(NULL, 10, self->value));
  Ptr_release(self);
  return num;
}

/* outside refcount system */
void Ratio_destroy(Ratio *self) {
  mpq_clear(self->value);
}

double Ratio_toDouble(Ratio *self) {
  double retVal = mpq_get_d(self->value);
  Ptr_release(self);
  return retVal;
}

void *Ratio_add(Ratio *self, Ratio *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  Ratio *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = Ratio_createUnassigned();
  mpq_add(retVal->value, self->value, other->value);
  if (selfReusable) Ptr_release(other);
  else if (otherReusable) Ptr_release(self);
  else { Ptr_release(self); Ptr_release(other); }
  return Ratio_simplify(retVal);
}

void *Ratio_sub(Ratio *self, Ratio *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  Ratio *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = Ratio_createUnassigned();
  mpq_sub(retVal->value, self->value, other->value);
  if (selfReusable) Ptr_release(other);
  else if (otherReusable) Ptr_release(self);
  else { Ptr_release(self); Ptr_release(other); }
  return Ratio_simplify(retVal);
}

void *Ratio_mul(Ratio *self, Ratio *other) {
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  Ratio *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = Ratio_createUnassigned();
  mpq_mul(retVal->value, self->value, other->value);
  if (selfReusable) Ptr_release(other);
  else if (otherReusable) Ptr_release(self);
  else { Ptr_release(self); Ptr_release(other); }
  return Ratio_simplify(retVal);
}

void *Ratio_div(Ratio *self, Ratio *other) {
  if (!mpq_cmp_si(other->value, 0, 1)) {
    Ptr_release(self);
    Ptr_release(other);
    return NULL; // Exception: divide by zero
  }
  bool selfReusable = Ptr_isReusable(self);
  bool otherReusable = Ptr_isReusable(other);
  Ratio *retVal;
  if (selfReusable) retVal = self;
  else if (otherReusable) retVal = other;
  else retVal = Ratio_createUnassigned();
  mpq_div(retVal->value, self->value, other->value);
  if (selfReusable) Ptr_release(other);
  else if (otherReusable) Ptr_release(self);
  else { Ptr_release(self); Ptr_release(other); }
  return Ratio_simplify(retVal);
}

bool Ratio_gte(Ratio *self, Ratio *other) {
  int cmp = mpq_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp >= 0;
}

bool Ratio_lt(Ratio *self, Ratio *other) {
  int cmp = mpq_cmp(self->value, other->value);
  Ptr_release(self);
  Ptr_release(other);
  return cmp < 0;
}
