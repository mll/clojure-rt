#include "Exception.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <stdio.h>

String *exceptionToString_C(void *exception);
void *createException_C(const char *className, String *message,
                        RTValue payload);
void deleteException_C(void *exception);

Exception *Exception_createAssertionErrorWithMessage(String *message) {
  Exception *self = (Exception *)allocate(sizeof(Exception));
  Object_create((Object *)self, exceptionType);
  self->bridgedData = createException_C("AssertionError", message, RT_boxNil());
  return self;
}

void Exception_destroy(Exception *self) {
  deleteException_C(self->bridgedData);
}

uword_t Exception_hash(Exception *self) { return (uword_t)self->bridgedData; }

bool Exception_equals(Exception *self, Exception *other) {
  return self->bridgedData == other->bridgedData;
}

String *Exception_toString(Exception *self) {
  String *retVal = exceptionToString_C(self->bridgedData);
  Ptr_release(self);
  return retVal;
}
