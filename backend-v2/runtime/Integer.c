#include "Integer.h"
#include "Hash.h"
#include "Object.h"
#include "RTValue.h"
#include "Ratio.h"
#include "String.h"
#include <stdio.h>
#include <stdlib.h>

/* mem done */
String *Integer_toString(RTValue self) {
  String *retVal = String_createDynamic(20);
  retVal->count = snprintf(retVal->value, 20, "%d", RT_unboxInt32(self));
  retVal->value[20] = '\0';
  String_recomputeHash(retVal);
  return retVal;
}

int64_t gcd(int64_t a, int64_t b) {
  while (b != 0) {
    a %= b;
    a ^= b;
    b ^= a;
    a ^= b;
  }
  return a;
}

// Integer or Ratio
RTValue Integer_div(int32_t num, int32_t den) {
  assert(den != 0 && "Internal error: Divide by zero.");
  if (!num)
    return RT_boxInt32(0);
  int32_t g = gcd(num, den);
  int32_t n = num / g;
  int32_t d = den / g;
  if (d == 1)
    return RT_boxInt32(n);
  if (d == -1)
    return RT_boxInt32(-n);
  return RT_boxPtr(Ratio_createFromInts(n, d));
}
