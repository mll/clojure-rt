#ifndef RT_SYMBOL
#define RT_SYMBOL

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "defines.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ObjectProto.h"
typedef struct String String;

struct Symbol {
  Object header;
  String *name;
  String *ns;
  RTValue metadata;
};
typedef struct Symbol Symbol;

Symbol *Symbol_create(String *string);
Symbol *Symbol_withMeta(Symbol *self, RTValue meta);
Symbol *Symbol_createWithMeta(String *string, RTValue meta);
String *Symbol_getName(Symbol *self);
void Symbol_destroy(Symbol *self);
bool Symbol_equals(Symbol *self, Symbol *other);
uword_t Symbol_hash(Symbol *self);
String *Symbol_toString(RTValue self);

#ifdef __cplusplus
}
#endif

#endif
