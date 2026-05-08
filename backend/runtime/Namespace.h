#ifndef RT_NAMESPACE_H
#define RT_NAMESPACE_H
#include "ObjectProto.h"
#include "RTValue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PersistentArrayMap PersistentArrayMap;
typedef struct String String;
typedef struct Class Class;
typedef struct Var Var;

struct Namespace {
  Object base;
  Symbol *name;
  _Atomic(RTValue) mappings; // points to PersistentArrayMap
  _Atomic(RTValue) aliases;  // points to PersistentArrayMap
};

Namespace *Namespace_create(Symbol *name);
Namespace *Namespace_find(Symbol *name);
Namespace *Namespace_findOrCreate(Symbol *name);
Symbol *Namespace_getName(Namespace *self);
void Namespace_destroy(Namespace *self, bool deallocateChildren);
uword_t Namespace_hash(Namespace *self);
bool Namespace_equals(Namespace *self, Namespace *other);
String *Namespace_toString(Namespace *self);
void Namespace_promoteToShared(Namespace *self, uword_t count);

RTValue Namespace_getMappings(Namespace *self);
RTValue Namespace_getAliases(Namespace *self);
bool Namespace_isInternedMapping(Namespace *self, Symbol *sym, RTValue o);
bool Namespace_checkReplacement(Namespace *self, Symbol *sym, RTValue oldVal, RTValue neuVal);
Var *Namespace_intern(Namespace *self, Symbol *sym);
RTValue Namespace_reference(Namespace *self, Symbol *sym, RTValue val);
RTValue Namespace_referenceClass(Namespace *self, Symbol *sym, Class *val);
void Namespace_unmap(Namespace *self, Symbol *sym);
Class *Namespace_importClassSym(Namespace *self, Symbol *sym, Class *c);
Var *Namespace_refer(Namespace *self, Symbol *sym, Var *var);
RTValue Namespace_getMapping(Namespace *self, Symbol *name);
Var *Namespace_findInternedVar(Namespace *self, Symbol *symbol);
Namespace *Namespace_lookupAlias(Namespace *self, Symbol *alias);
void Namespace_addAlias(Namespace *self, Symbol *alias, Namespace *ns);
void Namespace_removeAlias(Namespace *self, Symbol *alias);

#ifdef __cplusplus
}
#endif
#endif
