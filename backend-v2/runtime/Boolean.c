#include "Boolean.h"
#include "RTValue.h"
#include "String.h"
#include "Object.h"


/* mem done */
String *Boolean_toString(bool self) {  
  String *retVal = self ? String_create("true") : String_create("false");
  return retVal;
}

