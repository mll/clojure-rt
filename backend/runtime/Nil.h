#ifndef RT_NIL
#define RT_NIL
#include "String.h"
#include "defines.h"

struct Nil {
};

typedef struct Nil Nil;

Nil* Nil_create();
BOOL Nil_equals(Nil *self, Nil *other);
uint64_t Nil_hash(Nil *self);
String *Nil_toString(Nil *self); 
void Nil_destroy(Nil *self);
Nil *const Nil_getUnique();

void Nil_initialise();

#endif
