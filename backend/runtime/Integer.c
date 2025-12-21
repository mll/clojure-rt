#include "Integer.h"
#include "String.h"
#include "Object.h"
#include <stdlib.h>
#include <stdio.h>
#include "Hash.h"
#include "Ratio.h"


/* mem done */
String *Integer_toString(int32_t self) { 
  String *retVal = String_createDynamic(21);
  retVal->count = snprintf(retVal->value, 20, "%d", self);
  String_recomputeHash(retVal);
  return retVal;
}

int64_t gcd(int64_t a, int64_t b) {
  while (b != 0)
  {
      a %= b;
      a ^= b;
      b ^= a;
      a ^= b;
  }
  return a;
}

// Integer or Ratio
void* Integer_div(int64_t num, int64_t den) {
  if (!den) return NULL; // Exception: divide by zero
  if (!num) return Integer_create(0);
  int64_t g = gcd(num, den);
  int64_t n = num / g;
  int64_t d = den / g;
  if (d == 1) return Integer_create(n);
  if (d == -1) return Integer_create(-n);
  return Ratio_createFromInts(n, d);
}
