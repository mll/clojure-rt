#include "Double.h"
#include "String.h"
#include <stdio.h>
#include "Object.h"
#include "Hash.h"

/* mem done */
String *Double_toString(double self) {
  String *retVal = String_createDynamic(23);
  retVal->count = snprintf(retVal->value, 22, "%.17G", self); // TODO: print 5.0 instead of 5, print NaN, +/-Inf, anything else?
  String_recomputeHash(retVal);
  return retVal;
}

