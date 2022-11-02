#include "Object.h"
#include "String.h"
#include "sds/sds.h"


String* String_create(sds string) {
  Object *super = allocate(sizeof(String) + sizeof(Object)); 
  String *self = (String *)(super + 1);
  self->value = string;
  self->count = sdslen(string);
  Object_create(super, stringType);
  return self;
}

String* String_create_copy(char *string) {
  return String_create(sdsnew(string));
}

BOOL String_equals(String *self, String *other) {
  return sdscmp(self->value, other->value) == 0;
}

uint64_t String_hash(String *self) {
    sds str = self->value;
    uint64_t h = 5381;
    int32_t c;

    while ((c = *str++)) h = combineHash(h, c);
    return h;
}

String *String_toString(String *self) {
  retain(self);
  return self;
}

void String_destroy(String *self) {
  sdsfree(self->value);
}


String *String_concat(String *self, String *other) {
  sds new = sdsnew(self->value);
  new = sdscatsds(new, other->value);
  return String_create(new);
}
