#ifndef RT_BOOLEAN
#define RT_BOOLEAN
#include "String.h"
#include "defines.h"


struct Boolean {
  unsigned char value;
};

typedef struct Boolean Boolean;

Boolean* Boolean_create(BOOL initial);
BOOL Boolean_equals(Boolean *self, Boolean *other);
uint64_t Boolean_hash(Boolean *self);
String *Boolean_toString(Boolean *self); 
void Boolean_destroy(Boolean *self);

#endif
