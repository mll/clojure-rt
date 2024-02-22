#ifndef RT_INTEGER
#define RT_INTEGER
#include "String.h"
#include "defines.h"

struct Integer {
  int64_t value;
};

typedef struct Integer Integer;

Integer* Integer_create(int64_t integer);
BOOL Integer_equals(Integer *self, Integer *other);
uint64_t Integer_hash(Integer *self);
String *Integer_toString(Integer *self); 
void Integer_destroy(Integer *self);

void* Integer_div(int64_t num, int64_t den);

#endif
