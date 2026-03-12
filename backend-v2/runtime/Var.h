#ifndef RT_VAR
#define RT_VAR

#include "word.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "Keyword.h"
#include "RTValue.h"
#include "String.h"
#include "Nil.h"


typedef struct Object Object;

struct Var {
  Object super;
  bool unbound;
  bool dynamic;
  uword_t rev;
  RTValue root;
  RTValue keyword; // TODO: split name and namespace - Marek - why?

  // TODO: threadBound
};

typedef struct Var Var;

//Class *UNIQUE_UnboundClass;

Var *Var_create(RTValue keyword);
bool Var_equals(Var *self, Var *other);
uword_t Var_hash(Var *self);
String *Var_toString(Var *self);
void Var_destroy(Var *self);

Var *Var_setDynamic(Var *self, bool dynamic); // modifies and returns self
bool Var_isDynamic(Var *self);
bool Var_hasRoot(Var *self);
RTValue Var_deref(Var *self);
RTValue Var_bindRoot(Var *self, RTValue object);
RTValue Var_unbindRoot(Var *self);

#ifdef __cplusplus
}
#endif

#endif
