#include "Double.h"
#include "Hash.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* mem done */
String *Double_toString(RTValue self) {
  String *retVal = String_createDynamic(25);
  double val = RT_unboxDouble(self);
  if (isnan(val)) {
    retVal->count = snprintf(retVal->value, 25, "NaN");
  } else if (isinf(val)) {
    if (val < 0) {
      retVal->count = snprintf(retVal->value, 25, "-Infinity");
    } else {
      retVal->count = snprintf(retVal->value, 25, "Infinity");
    }
  } else {
    retVal->count = snprintf(retVal->value, 25, "%.17G", val);
    if (!strchr(retVal->value, '.') && !strchr(retVal->value, 'E') && !strchr(retVal->value, 'e')) {
      retVal->count += snprintf(retVal->value + retVal->count, 25 - retVal->count, ".0");
    }
  }
  retVal->value[retVal->count] = '\0';
  String_recomputeHash(retVal);
  return retVal;
}
