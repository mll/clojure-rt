#include "Double.h"
#include "String.h"
#include <stdio.h>
#include "Object.h"
#include "sds/sds.h"

Double* Double_create(double d) {
  Object *super = allocate(sizeof(Double) + sizeof(Object)); 
  Double *self = (Double *)(super + 1);
  self->value = d;
  Object_create(super, doubleType);
  return self;
}

BOOL Double_equals(Double *self, Double *other) {
  return self->value == other->value;
}

uint64_t Double_hash(Double *self) {
  uint64_t v = *((uint64_t *) &(self->value)); 
  return (uint64_t)(v^(v>>32));
}

String *Double_toString(Double *self) {
  return String_create(sdscatprintf(sdsempty(), "%f", self->value));
}

void Double_destroy(Double *self) {
}
