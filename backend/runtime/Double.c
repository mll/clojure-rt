#include "Double.h"
#include "String.h"
#include <stdio.h>
#include "Object.h"
#include "Hash.h"

/* mem done */
Double* Double_create(double d) {
  Double *self = (Double *)allocate(sizeof(Double)); 
  self->value = d;
  Object_create((Object *)self, doubleType);
  return self;
}

/* outside refcount system */
BOOL Double_equals(Double *self, Double *other) {
  return self->value == other->value;
}

/* outside refcount system */
uint64_t Double_hash(Double *self) {
  uint64_t v = *((uint64_t *) &(self->value)); 
  return combineHash(5381, avalanche_64(v));
}

/* mem done */
String *Double_toString(Double *self) {
  String *retVal = String_createDynamic(23);
  retVal->count = snprintf(retVal->value, 22, "%.17G", self->value); // TODO: print 5.0 instead of 5, print NaN, +/-Inf, anything else?
  String_recomputeHash(retVal);
  release(self);
  return retVal;
}

/* outside refcount system */
void Double_destroy(Double *self) {
}
