#include "Object.h"
#include "Function.h"
#include "Integer.h"
#include "Hash.h"
#include "String.h"
#include <stdarg.h>

/* mem done */
ClojureFunction* Function_create(uint64_t methodCount, uint64_t uniqueId, uint64_t maxArity, BOOL once) {
  size_t size = sizeof(ClojureFunction) + methodCount * sizeof(FunctionMethod);
  ClojureFunction *self = (ClojureFunction *)allocate(size);
  memset(self, 0, size);
  self->methodCount = methodCount;
  self->maxArity = maxArity;
  self->uniqueId = uniqueId;
  self->once = once;
  self->executed = FALSE;
  Object_create((Object *)self, functionType);
  // TODO - ^:once meta. When present 
  return self;
}

/* outside refcount system (w.r.t. self) and closedOvers (they need to be retained already when this fun is called) */
void Function_fillMethod(ClojureFunction *self, uint64_t position, uint64_t index, uint64_t fixedArity, BOOL isVariadic, char *loopId, int64_t closedOversCount, ...) {
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
BOOL Function_validCallWithArgCount(ClojureFunction *self, uint64_t argCount) {
  if (argCount <= self->maxArity) {
    for (uint64_t i = 0; i < self->methodCount; ++i) {
      if (self->methods[i].fixedArity == argCount && !self->methods[i].isVariadic) {
        return TRUE;
      }
    }
  } else {
    for (uint64_t i = 0; i < self->methodCount; ++i) {
      if (self->methods[i].isVariadic) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

/* outside refcount system */
BOOL Function_equals(ClojureFunction *self, ClojureFunction *other) {
  return self->uniqueId == other->uniqueId;
}

/* outside refcount system */
uint64_t Function_hash(ClojureFunction *self) {
  return avalanche_64(self->uniqueId);
}

/* mem done */
String *Function_toString(ClojureFunction *self) {
  Integer *i = Integer_create(self->uniqueId);
  release(self);
  return String_concat(String_createStatic("fn_"), toString(i));
} 

void Function_cleanupOnce(ClojureFunction *self) {
  assert(!self->executed && "Function with :once meta executed more than once");
  self->executed = TRUE;
  for(int i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(int j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]); 
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
}


/* outside refcount system */
void Function_destroy(ClojureFunction *self) {
  if(self->once && self->executed) return;

  for(int i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(int j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]);
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
  
}


