#ifndef RT_STRING_BUILDER
#define RT_STRING_BUILDER

#ifdef __cplusplus
extern "C" {
#endif

#include "defines.h"
#include "word.h"
#include <stdbool.h>

typedef struct Object Object;
typedef struct String String;

struct StringBuilder {
  Object super;
  String *value;
};

typedef struct StringBuilder StringBuilder;

StringBuilder *StringBuilder_create(String *str);
StringBuilder *StringBuilder_append(StringBuilder *self, String *str_to_append);
String *StringBuilder_toString(StringBuilder *self);

bool StringBuilder_equals(StringBuilder *self, StringBuilder *other);
uword_t StringBuilder_hash(StringBuilder *self);
void StringBuilder_destroy(StringBuilder *self);

#ifdef __cplusplus
}
#endif

#endif
