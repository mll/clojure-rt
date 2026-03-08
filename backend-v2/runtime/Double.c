#include "Double.h"
#include "Hash.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <stdio.h>

/* mem done */
String *Double_toString(RTValue self) {
  String *retVal = String_createDynamic(25);
  retVal->count =
      snprintf(retVal->value, 25, "%.17G",
               RT_unboxDouble(self)); // TODO: print 5.0 instead of 5, print
                                      // NaN, +/-Inf, anything else?
  retVal->value[25] = '\0';
  String_recomputeHash(retVal);
  return retVal;
}
