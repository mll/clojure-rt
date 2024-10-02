#ifndef RT_NIL
#define RT_NIL
#include "String.h"
#include "defines.h"

struct Nil {
};

typedef struct Nil Nil;

Nil *UNIQUE_NIL;

Nil* Nil_create();
Nil* Nil_create1(void * obj);
Nil* Nil_create2(void * obj1, void * obj2);
Nil* Nil_createN(uint64_t objCount, ...);
BOOL Nil_equals(Nil *self, Nil *other);
uint64_t Nil_hash(Nil *self);
String *Nil_toString(Nil *self); 
void Nil_destroy(Nil *self);

void Nil_initialise();

#endif
