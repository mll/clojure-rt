struct ExecutionContext;
#ifndef RT_VAR
#define RT_VAR

#include "word.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "Keyword.h"
#include "Nil.h"
#include "RTValue.h"
#include "String.h"

typedef struct Object Object;
struct Namespace;
struct Symbol;

struct Var {
  Object super;
  bool dynamic;
  _Atomic(uword_t) rev;
  _Atomic(RTValue) root;
  RTValue keyword; // TODO: split name and namespace - Marek - why?
  struct Namespace *ns;
  struct Symbol *sym;
  _Atomic(RTValue) metadata;

  // TODO: threadBound
};

typedef struct Var Var;

// Class *UNIQUE_UnboundClass;

Var *Var_create(RTValue keyword);
Var *Var_create_interned(struct Namespace *ns, struct Symbol *sym);
Var *Var_resetMeta(Var *self, RTValue meta);
RTValue Var_getMeta(Var *self);
bool Var_equals(Var *self, Var *other);
uword_t Var_hash(Var *self);
String *Var_toString(Var *self);
void Var_destroy(Var *self);

Var *Var_setDynamic(Var *self, bool dynamic); // modifies and returns self
bool Var_isDynamic(Var *self);
bool Var_hasRoot(Var *self);
RTValue Var_deref(__attribute__((swift_context)) struct ExecutionContext *ctx, Var *self) __attribute__((swiftcall));
/* the returned reference is not retained and is not guaranteed to even
   be valid after the call returns. */
RTValue Var_peek(__attribute__((swift_context)) struct ExecutionContext *ctx, Var *self) __attribute__((swiftcall));
RTValue Var_set(__attribute__((swift_context)) struct ExecutionContext *ctx, Var *self, RTValue value) __attribute__((swiftcall));

RTValue Var_bindRoot(Var *self, RTValue object);
RTValue Var_unbindRoot(Var *self);

#ifdef __cplusplus
}
#endif

#endif
