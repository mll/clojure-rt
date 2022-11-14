#include "Object.h"
#include "Function.h"

Function* Function_create(uint64_t methodCount, char *uniqueName, uint64_t hash, uint64_t maxArity) {
  Object *super = allocate(sizeof(Function) + sizeof(Object) + methodCount * sizeof(FunctionMethod)); 
  Function *self = (Function *)(super + 1);
  self->methodCount = methodCount;
  self->uniqueName = uniqueName;
  self->hash = hash;
  self->maxArity = maxArity;
  Object_create(super, functionType);
  return self;
}

void Function_fillMethod(Function *self, uint64_t position, uint64_t fixedArity,  BOOL isVariadic,  char *loopId, void *genericBootstrapPointer) {
  FunctionMethod * method = self->methods + position;
  method->fixedArity = fixedArity;
  method->isVariadic = isVariadic;
  method->loopId = loopId;
  method->genericBootstrapPointer = genericBootstrapPointer;
}

BOOL Function_equals(Function *self, Function *other) {
  return strcmp(self->uniqueName, other->uniqueName) == 0;
}

uint64_t Function_hash(Function *self) {
  return self->hash;
}
String *Function_toString(Function *self) {
  return String_create(self->uniqueName);
} 

void Function_destroy(Function *self) {
}


