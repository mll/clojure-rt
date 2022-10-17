#ifndef RT_DOUBLE
#define RT_DOUBLE
#include "Object.h"
#include "String.h"
#include "defines.h"

struct Double {
  double value;
};

typedef struct Double Double;

Double* Double_create(double d);
bool Double_equals(Double *self, Double *other);
uint64_t Double_hash(Double *self);
String *Double_toString(Double *self); 
void Double_destroy(Double *self);

#endif
