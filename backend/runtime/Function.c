#include "Object.h"
#include "Function.h"
#include "Integer.h"
#include "Hash.h"
#include "String.h"

/* mem done */
Function* Function_create(uint64_t methodCount, uint64_t uniqueId, uint64_t maxArity) {
  size_t size = sizeof(Function) + sizeof(Object) + methodCount * sizeof(FunctionMethod);
  Object *super = allocate(size); 
  memset(super, 0, size);
  Function *self = (Function *)(super + 1);
  self->methodCount = methodCount;
  self->maxArity = maxArity;
  self->uniqueId = uniqueId;
  Object_create(super, functionType);
  return self;
}

/* outside refcount system */
void Function_fillMethod(Function *self, uint64_t position, uint64_t index, uint64_t fixedArity,  BOOL isVariadic,  char *loopId) {
  FunctionMethod * method = self->methods + position;
  method->fixedArity = fixedArity;
  method->isVariadic = isVariadic;
  method->loopId = loopId;
  method->index = index;
}

/* outside refcount system */
BOOL Function_equals(Function *self, Function *other) {
  return self->uniqueId == other->uniqueId;
}

/* outside refcount system */
uint64_t Function_hash(Function *self) {
  return avalanche_64(self->uniqueId);
}

/* mem done */
String *Function_toString(Function *self) {
  Integer *i = Integer_create(self->uniqueId);
  release(self);
  return String_concat(String_createStatic("fn_"), toString(i));
} 

/* outside refcount system */
void Function_destroy(Function *self) {
}


