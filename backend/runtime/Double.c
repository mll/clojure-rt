#include "Double.h"
#include "Object.h"
#include "String.h"
#include <stdio.h>

Double* Double_create(double d) {
  Object *super = allocate(sizeof(Double) + sizeof(Object)); 
  Double *self = (Double *)(super + 1);
  self->value = d;
  Object_create(super, doubleType);
  return self;
}

bool Double_equals(Double *self, Double *other) {
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
