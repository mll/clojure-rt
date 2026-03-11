#ifndef RT_STRING
#define RT_STRING

#ifdef __cplusplus
extern "C" {
#endif

#include "PersistentVectorIterator.h"
#include "RTValue.h"
#include "defines.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Object Object;
typedef struct PersistentVector PersistentVector;

enum specialisedString { staticString, dynamicString, compoundString };

typedef enum specialisedString specialisedString;

struct String {
  Object super;
  specialisedString specialisation;
  uword_t count;
  uword_t hash;
  char value[];
};

typedef struct String String;

struct StringIterator {
  uword_t index;
  uword_t inBlockIndex;
  uword_t blockLength;
  String *current;
  PersistentVectorIterator iterator;
  char *block;
};

typedef struct StringIterator StringIterator;

String *String_createDynamicStr(const char *str);
String *String_createStaticOptimised(char *string, uword_t len, uword_t hash);
String *String_create(const char *string);
String *String_createDynamic(size_t size);
String *String_createStatic(const char *string);

StringIterator String_iterator(String *self);
char *String_iteratorGet(StringIterator *it);
char *String_iteratorNext(StringIterator *it);

String *String_concat(String *self, String *other);
/* Creates a version of the string that has guaranteed 'value' field */
String *String_compactify(String *self);
const char *String_c_str(String *self);
uword_t String_computeHash(const char *str);
void String_recomputeHash(String *s);

bool String_equals(String *self, String *other);
uword_t String_hash(String *self);
String *String_toString(String *self);
void String_destroy(String *self);

bool String_contains(String *self, String *other);
word_t String_indexOf(String *self, String *other);
word_t String_indexOfFrom(String *self, String *other, word_t fromIndex);

String *String_replace(String *self, String *target, String *replacement);
char String_charAt(String *self, uword_t index);
String *String_subs(String *self, uword_t start, uword_t end);

#ifdef __cplusplus
}
#endif

#endif
