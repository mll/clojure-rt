#ifndef RT_STRING
#define RT_STRING

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "defines.h"

typedef struct Object Object; 
typedef struct PersistentVector PersistentVector; 


enum specialisedString {
  staticString,
  dynamicString,
  compoundString
};

typedef enum specialisedString specialisedString;

struct String {
  uint64_t count;
  uint64_t hash;
  specialisedString specialisation;
  char value[]; 
};

typedef struct String String; 

struct StringIterator {
  uint64_t index;
  uint64_t inBlockIndex;
  uint64_t blockIndex;
  uint64_t blockLength;
  char *block; 
};

typedef struct StringIterator StringIterator; 

String* String_createStaticOptimised(char *string, uint64_t len, uint64_t hash);
String* String_create(char *string);
String* String_createDynamic(size_t size);


StringIterator String_iterator(String *self);
char String_iteratorGetChar(String *self, StringIterator *it);
char String_iteratorNext(String *self, StringIterator *it);

String *String_concat(String *self, String *other);
/* Destroys self, but not other! */
String *String_append(String *self, String *other);
/* Creates a version of the string that has guaranteed value */
String *String_compactify(String *self);
char *String_c_str(String *self);
void String_recomputeHash(String *s);


BOOL String_equals(String *self, String *other);
uint64_t String_hash(String *self);
String *String_toString(String *self);
void String_destroy(String *self);




#endif
