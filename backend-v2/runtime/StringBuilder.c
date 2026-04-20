#include "Object.h"
#include "String.h"
#include "StringBuilder.h"
#include <assert.h>
#include <string.h>

StringBuilder *StringBuilder_create(String *str) {
  StringBuilder *self = (StringBuilder *)allocate(sizeof(StringBuilder));
  self->value = str;
  Object_create((Object *)self, stringBuilderType);
  return self;
}

StringBuilder *StringBuilder_append(StringBuilder *self,
                                    String *str_to_append) {
  self->value = String_concat(self->value, str_to_append);
  return self;
}

String *StringBuilder_toString(StringBuilder *self) {
  self->value = String_compactify(self->value);
  Ptr_retain(self->value);
  String *res = self->value;
  Ptr_release(self);
  return res;
}

bool StringBuilder_equals(StringBuilder *self, StringBuilder *other) {
  return String_equals(self->value, other->value);
}

uword_t StringBuilder_hash(StringBuilder *self) {
  return String_hash(self->value);
}

void StringBuilder_destroy(StringBuilder *self) {
  Ptr_release(self->value);
}
