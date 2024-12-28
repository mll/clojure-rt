#ifndef RT_VAR
#define RT_VAR

#include "defines.h"
#include "Object.h"
#include "Nil.h"

typedef struct Object Object;
typedef struct String String;
typedef struct Keyword Keyword;

struct Var {
  Object super;
  BOOL unbound;
  BOOL dynamic;
  uint64_t rev;
  Object *root;
  Keyword *keyword; // TODO: split name and namespace - Marek - why?

  // TODO: threadBound
};

typedef struct Var Var;

Class *UNIQUE_UnboundClass;

Var *Var_create(Keyword *keyword);
BOOL Var_equals(Var *self, Var *other);
uint64_t Var_hash(Var *self);
String *Var_toString(Var *self);
void Var_destroy(Var *self);

Var *Var_setDynamic(Var *self, BOOL dynamic); // modifies and returns self
BOOL Var_isDynamic(Var *self);
BOOL Var_hasRoot(Var *self);
void *Var_deref(Var *self);
Nil *Var_bindRoot(Var *self, void *object);
Nil *Var_unbindRoot(Var *self);

#endif
