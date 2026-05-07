#include "Nil.h"
#include "String.h"
#include <stdio.h>
#include <stdarg.h>
#include "Object.h"

/* mem done */
String *Nil_toString() {
  return String_create("nil");
}

