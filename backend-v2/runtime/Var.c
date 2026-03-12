#include "Var.h"
#include "Exceptions.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include "Hash.h"
#include "word.h"
#include <stdarg.h>

// TODO: UnboundClass is printed in different way
//Class *UNIQUE_UnboundClass;

Var *Var_create(RTValue keyword) {
  Var *self = (Var *)allocate(sizeof(Var));
  self->unbound = true;
  retain(keyword);
//  retain(UNIQUE_UnboundClass);
  self->root = RT_boxNull(); //(Object *)Deftype_create(UNIQUE_UnboundClass, 1, keyword);
  self->dynamic = false;
  self->keyword = keyword;
  self->rev = 0;
  Object_create((Object *)self, varType);
  return self;
};

bool Var_equals(Var *self, Var *other) {
  return false; // pointer equality in Object_equals
};

uword_t Var_hash(Var *self) {
  return combineHash(hash(self->keyword), hash(self->root));
};

String *Var_toString(Var *self) {
  String *retVal = String_create("#");
  retVal = String_concat(retVal, String_replace(toString(self->keyword),
                                                String_createStatic(":"),
                                                String_createStatic("'")));
  
  Ptr_release(self);
  return retVal;
};

void Var_destroy(Var *self) {
  release(self->root);
  release(self->keyword);
};

Var *Var_setDynamic(Var *self, bool dynamic) { // modifies and returns self
  self->dynamic = dynamic;
  return self;
};

bool Var_isDynamic(Var *self) {
  bool retVal = self->dynamic;
  Ptr_release(self);
  return retVal;
};

bool Var_hasRoot(Var *self) {
  bool retVal = !self->unbound;
  Ptr_release(self);
  return retVal;
};

RTValue Var_deref(Var *self) { // TODO: synchronized
  RTValue retVal = self->root;
  // TODO: threadBound
  retain(retVal);
  Ptr_release(self);
  return retVal;
};

RTValue Var_bindRoot(Var *self, RTValue object) { // TODO: synchronized
  RTValue oldRoot = self->root;
  ++self->rev;
  self->unbound = false;
  self->root = object;
  release(oldRoot);
  Ptr_release(self);
  return RT_boxNil();
}

RTValue Var_unbindRoot(Var *self) { // TODO: synchronized - Marek: What does it exactly mean?
  RTValue oldRoot = self->root;
  ++self->rev;
  self->unbound = true;
//  retain(self->keyword);
//  retain(UNIQUE_UnboundClass);
  self->root = RT_boxNull(); // Deftype_create(UNIQUE_UnboundClass, 1, self->keyword);
  release(oldRoot);
  Ptr_release(self);
  return RT_boxNil();
}
