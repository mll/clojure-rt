#include "String.h"
#include "Object.h"

String* String_create(sds string) {
  String *self = malloc(sizeof(String));
  self->value = string;
  self->count = sdslen(string);
  self->super = Object_create(stringType, self);
  return self;
}

bool String_equals(String *self, String *other) {
  return sdscmp(self->value, other->value) == 0;
}

uint64_t String_hash(String *self) {
    sds str = self->value;
    uint64_t h = 5381;
    int64_t c;

    while ((c = *str++))
        h = ((h << 5) + h) + c; /* hash * 33 + c */

    return h;
}

String *String_toString(String *self) {
  retain(self->super);
  return self;
}

void String_destroy(String *self) {
  sdsfree(self->value);
}



