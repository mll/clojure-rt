#include "Object.h"
#include "Function.h"
#include "Integer.h"
#include "Hash.h"
#include "String.h"
#include <stdarg.h>

/* mem done */
Function* Function_create(uint64_t methodCount, uint64_t uniqueId, uint64_t maxArity, BOOL once) {
  size_t size = sizeof(Function) + sizeof(Object) + methodCount * sizeof(FunctionMethod);
  Object *super = allocate(size); 
  memset(super, 0, size);
  Function *self = (Function *)(super + 1);
  self->methodCount = methodCount;
  self->maxArity = maxArity;
  self->uniqueId = uniqueId;
  self->once = once;
  self->executed = FALSE;
  Object_create(super, functionType);
  // TODO - ^:once meta. When present 
  return self;
}

/* outside refcount system (w.r.t. self) and closedOvers (they need to be retained already when this fun is called) */
void Function_fillMethod(Function *self, uint64_t position, uint64_t index, uint64_t fixedArity,  BOOL isVariadic, char *loopId, int64_t closedOversCount, ...) {
  FunctionMethod * method = self->methods + position;
  method->fixedArity = fixedArity;
  method->isVariadic = isVariadic;
  method->loopId = loopId;
  method->index = index;
  method->closedOversCount = closedOversCount;
  if(closedOversCount > 0) method->closedOvers = allocate(closedOversCount * sizeof(void *));
  va_list args;
  va_start(args, closedOversCount);
  for(int i=0; i<closedOversCount;i++) {
    void *closedOver = va_arg(args, void *); 
    method->closedOvers[i] = closedOver;
  }  
  va_end(args);
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

void Function_cleanupOnce(Function *self) {
  assert(!self->executed && "Function with :once meta executed more than once");
  self->executed = TRUE;
  for(int i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(int j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]); 
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
}


/* outside refcount system */
void Function_destroy(Function *self) {
  if(self->once && self->executed) return;

  for(int i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(int j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]);
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
  
}


