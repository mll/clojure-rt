#include "Object.h"
#include "Var.h"
#include "Nil.h"
#include "Hash.h"
#include <stdarg.h>

// TODO: UnboundClass is printed in different way
Class *UNIQUE_UnboundClass;

Var *Var_create(Keyword *keyword) {
  Var *self = (Var *)allocate(sizeof(Var));
  self->unbound = TRUE;
  retain(keyword);
  retain(UNIQUE_UnboundClass);
  self->root = (Object *)Deftype_create(UNIQUE_UnboundClass, 1, keyword);
  self->dynamic = FALSE;
  self->keyword = keyword;
  self->rev = 0;
  Object_create((Object *)self, varType);
  return self;
};

BOOL Var_equals(Var *self, Var *other) {
  return FALSE; // pointer equality in Object_equals
};

uint64_t Var_hash(Var *self) {
  return combineHash(hash(self->keyword), Object_hash(self->root));
};

String *Var_toString(Var *self) {
  String *retVal = String_create("#'");
  retain(self->keyword->string);
  retVal = String_concat(retVal, self->keyword->string);
  release(self);
  return retVal;
};

void Var_destroy(Var *self) {
  if (self->root) Object_release(self->root);
  release(self->keyword);
};

Var *Var_setDynamic(Var *self, BOOL dynamic) { // modifies and returns self
  self->dynamic = dynamic;
  return self;
};

BOOL Var_isDynamic(Var *self) {
  BOOL retVal = self->dynamic;
  release(self);
  return retVal;
};

BOOL Var_hasRoot(Var *self) {
  BOOL retVal = !self->unbound;
  release(self);
  return retVal;
};

void *Var_deref(Var *self) { // TODO: synchronized
  void *retVal = self->root;
  // TODO: threadBound
  retain(retVal);
  release(self);
  return retVal;
};

Nil *Var_bindRoot(Var *self, void *object) { // TODO: synchronized
  Object *oldRoot = self->root;
  ++self->rev;
  self->unbound = FALSE;
  self->root = (Object *)object;
  Object_release(oldRoot);
  release(self);
  retain(UNIQUE_NIL);
  return UNIQUE_NIL;
}

Nil *Var_unbindRoot(Var *self) { // TODO: synchronized - Marek: What does it exactly mean?
  Object *oldRoot = self->root;
  ++self->rev;
  self->unbound = TRUE;
  retain(self->keyword);
  retain(UNIQUE_UnboundClass);
  self->root = (Object *)Deftype_create(UNIQUE_UnboundClass, 1, self->keyword);
  Object_release(oldRoot);
  release(self);
  retain(UNIQUE_NIL);
  return UNIQUE_NIL;
}
