#include "Boolean.h"
#include "RTValue.h"
#include "String.h"
#include "Object.h"


/* mem done */
String *Boolean_toString(RTValue self) {  
  String *retVal = RT_unboxBool(self) ? String_create("true") : String_create("false");
  return retVal;
}

