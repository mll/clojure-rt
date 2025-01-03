#ifndef RT_STRING
#define RT_STRING

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"
#include "PersistentVectorIterator.h"

typedef struct Object Object; 
typedef struct PersistentVector PersistentVector; 


enum specialisedString {
  staticString,
  dynamicString,
  compoundString
};

typedef enum specialisedString specialisedString;

struct String {
  Object super;
  specialisedString specialisation;
  uint64_t count;
  uint64_t hash;
  char value[]; 
};

typedef struct String String; 

struct StringIterator {
  uint64_t index;
  uint64_t inBlockIndex;
  uint64_t blockLength;
  String *current;
  PersistentVectorIterator iterator;
  char *block; 
};

typedef struct StringIterator StringIterator; 

String* String_createDynamicStr(const char *str);
String* String_createStaticOptimised(char *string, uint64_t len, uint64_t hash);
String* String_create(char *string);
String* String_createDynamic(size_t size);
String* String_createStatic(char *string);

StringIterator String_iterator(String *self);
char *String_iteratorGet(StringIterator *it);
char *String_iteratorNext(StringIterator *it);

String *String_concat(String *self, String *other);
/* Creates a version of the string that has guaranteed 'value' field */
String *String_compactify(String *self);
char *String_c_str(String *self);
void String_recomputeHash(String *s);

BOOL String_equals(String *self, String *other);
uint64_t String_hash(String *self);
String *String_toString(String *self);
void String_destroy(String *self);

BOOL String_contains(String *self, String *other);
int64_t String_indexOf(String *self, String *other);
int64_t String_indexOfFrom(String *self, String *other, int64_t fromIndex);

String *String_replace(String *self, String *target, String *replacement);

#endif
