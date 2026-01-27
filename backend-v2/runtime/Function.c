#include "Object.h"
#include "Function.h"
#include "Integer.h"
#include "Hash.h"
#include "RTValue.h"
#include "String.h"
#include <stdarg.h>


/* mem done */
ClojureFunction* Function_create(uword_t methodCount, uword_t uniqueId, uword_t maxArity, bool once) {
  size_t size = sizeof(ClojureFunction) + methodCount * sizeof(FunctionMethod);
  ClojureFunction *self = (ClojureFunction *)allocate(size);
  memset(self, 0, size);
  self->methodCount = methodCount;
  self->maxArity = maxArity;
  self->uniqueId = uniqueId;
  self->once = once;
  self->executed = false;
  Object_create((Object *)self, functionType);
  // TODO - ^:once meta. When present 
  return self;
}

/* outside refcount system (w.r.t. self) and closedOvers (they need to be retained already when this fun is called) */
void Function_fillMethod(ClojureFunction *self, uword_t position, uword_t index, uword_t fixedArity, bool isVariadic, char *loopId, word_t closedOversCount, ...) {
  FunctionMethod * method = self->methods + position;
  method->fixedArity = fixedArity;
  method->isVariadic = isVariadic;
  method->loopId = loopId;
  method->index = index;
  method->closedOversCount = closedOversCount;
  if(closedOversCount > 0) method->closedOvers = allocate(closedOversCount * sizeof(RTValue));
  va_list args;
  va_start(args, closedOversCount);
  for(word_t i=0; i<closedOversCount;i++) {
    RTValue closedOver = va_arg(args, RTValue); 
    method->closedOvers[i] = closedOver;
  }
  va_end(args);
}

/* outside refcount system */
bool Function_validCallWithArgCount(ClojureFunction *self, uword_t argCount) {
  if (argCount <= self->maxArity) {
    for (uword_t i = 0; i < self->methodCount; ++i) {
      if (self->methods[i].fixedArity == argCount && !self->methods[i].isVariadic) {
        return true;
      }
    }
  } else {
    for (uword_t i = 0; i < self->methodCount; ++i) {
      if (self->methods[i].isVariadic) {
        return true;
      }
    }
  }
  return false;
}

/* outside refcount system */
bool Function_equals(ClojureFunction *self, ClojureFunction *other) {
  return self->uniqueId == other->uniqueId;
}

/* outside refcount system */
uword_t Function_hash(ClojureFunction *self) {
  return avalanche(self->uniqueId);
}

/* mem done */
String *Function_toString(ClojureFunction *self) {
  String *retVal = String_concat(String_createStatic("fn_"), toString(RT_boxInt32(self->uniqueId)));
  Ptr_release(self);
  return retVal;
} 

void Function_cleanupOnce(ClojureFunction *self) {
  assert(!self->executed && "Function with :once meta executed more than once");
  self->executed = true;
  for(uword_t i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(uword_t j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]); 
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
}


/* outside refcount system */
void Function_destroy(ClojureFunction *self) {
  if(self->once && self->executed) return;

  for(uword_t i=0; i < self->methodCount; i++) {
    FunctionMethod *method = &(self->methods[i]);
    for(uword_t j=0; j < method->closedOversCount; j++) release(method->closedOvers[j]);
    if(method->closedOversCount) deallocate(method->closedOvers);
  }
  
}


