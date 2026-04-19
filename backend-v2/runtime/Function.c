#include "Object.h"
#include "Function.h"
#include "Integer.h"
#include "Hash.h"
#include "RTValue.h"
#include "String.h"
#include "Exceptions.h"
#include "PersistentList.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


/* mem done */
ClojureFunction* Function_create(uword_t methodCount, uword_t maxArity, bool once) {
  size_t size = sizeof(ClojureFunction) + methodCount * sizeof(FunctionMethod);
  ClojureFunction *self = (ClojureFunction *)allocate(size);
  memset(self, 0, size);
  self->methodCount = methodCount;
  self->maxArity = maxArity;
  self->once = once;
  self->executed = false;
  Object_create((Object *)self, functionType);
  return self;
}

/* outside refcount system (w.r.t. self) and closedOvers (they need to be retained already when this fun is called) */
void Function_fillMethod(ClojureFunction *self, uword_t position, uword_t index, uword_t fixedArity, bool isVariadic, void *implementation, char *loopId, word_t closedOversCount, ...) {
  FunctionMethod * method = self->methods + position;
  method->fixedArity = fixedArity;
  method->isVariadic = isVariadic;
  method->baselineImplementation = implementation;
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
  return self == other;
}

/* outside refcount system */
uword_t Function_hash(ClojureFunction *self) {
  return avalanche((uword_t)self);
}

/* mem done */
String *Function_toString(ClojureFunction *self) {
  char buf[2048];
  int pos = snprintf(buf, sizeof(buf), "fn_%p {", (void *)self);
  for (uword_t i = 0; i < self->methodCount; ++i) {
    FunctionMethod *m = &self->methods[i];
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    "%s[arity %d, %s, closed-overs %d, impl %p]", (i > 0 ? ", " : ""),
                    (int)m->fixedArity, (m->isVariadic ? "var" : "fixed"),
                    (int)m->closedOversCount, m->baselineImplementation);
    if (pos >= (int)sizeof(buf) - 64)
      break; // Avoid overflow or very long strings
  }
  snprintf(buf + pos, sizeof(buf) - pos, "}");
  String *retVal = String_createDynamicStr(buf);
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
RTValue RT_invokeDynamic(RTValue funObj, RTValue *args, int32_t argCount) {
  (void)funObj; (void)args; (void)argCount;
  fprintf(stderr, "RT_invokeDynamic: Not yet implemented\n");
  abort();
}

FunctionMethod *Function_extractMethod(RTValue funObj, uword_t argCount) {
  if (getType(funObj) != functionType) {
    throwIllegalArgumentException_C("Not a function");
  }

  ClojureFunction *self = (ClojureFunction *)RT_unboxPtr(funObj);

  // 1. Exact match
  for (uword_t i = 0; i < self->methodCount; ++i) {
    if (!self->methods[i].isVariadic && self->methods[i].fixedArity == argCount) {
      return &self->methods[i];
    }
  }

  // 2. Variadic match
  for (uword_t i = 0; i < self->methodCount; ++i) {
    if (self->methods[i].isVariadic && argCount >= self->methods[i].fixedArity) {
      return &self->methods[i];
    }
  }

  throwArityException_C(-1, (int)argCount);
}

RTValue RT_packVariadic(uword_t argCount, RTValue *args, uword_t fixedArity) {
  if (argCount <= fixedArity) return RT_boxNil();
  return RT_createListFromArray((int32_t)(argCount - fixedArity), args + fixedArity);
}

/* --- Universal Callable Bridges --- */

static RTValue Baseline_Keyword_Invoke_1(Frame *frame, RTValue arg0, RTValue a1, RTValue a2, RTValue a3, RTValue a4) {
    return Keyword_invoke(frame->self, arg0, RT_boxNil());
}

static RTValue Baseline_Keyword_Invoke_2(Frame *frame, RTValue arg0, RTValue arg1, RTValue a2, RTValue a3, RTValue a4) {
    return Keyword_invoke(frame->self, arg0, arg1);
}

static RTValue Baseline_Map_Invoke_1(Frame *frame, RTValue arg0, RTValue a1, RTValue a2, RTValue a3, RTValue a4) {
    return PersistentArrayMap_dynamic_get(frame->self, arg0);
}

static RTValue Baseline_Vector_Invoke_1(Frame *frame, RTValue arg0, RTValue a1, RTValue a2, RTValue a3, RTValue a4) {
    return PersistentVector_dynamic_nth((PersistentVector *)RT_unboxPtr(frame->self), arg0);
}

// Global method descriptors for non-function callables
struct FunctionMethod Global_Keyword_Method_1 = { .fixedArity = 1, .isVariadic = false, .baselineImplementation = Baseline_Keyword_Invoke_1 };
struct FunctionMethod Global_Keyword_Method_2 = { .fixedArity = 2, .isVariadic = false, .baselineImplementation = Baseline_Keyword_Invoke_2 };
struct FunctionMethod Global_Map_Method_1     = { .fixedArity = 1, .isVariadic = false, .baselineImplementation = Baseline_Map_Invoke_1     };
struct FunctionMethod Global_Vector_Method_1  = { .fixedArity = 1, .isVariadic = false, .baselineImplementation = Baseline_Vector_Invoke_1  };

struct FunctionMethod *RT_updateICSlot(void *slot, RTValue currentVal,
                                     uint64_t argCount) {
  objectType type = getType(currentVal);
  struct FunctionMethod *method = NULL;

  if (type == functionType) {
    method = Function_extractMethod(currentVal, argCount);
  } else if (type == keywordType) {
    if (argCount == 1) method = &Global_Keyword_Method_1;
    else if (argCount == 2) method = &Global_Keyword_Method_2;
  } else if (type == persistentArrayMapType) {
    if (argCount == 1) method = &Global_Map_Method_1;
  } else if (type == persistentVectorType) {
    if (argCount == 1) method = &Global_Vector_Method_1;
  }

  if (!method) {
    throwIllegalStateException_C("Attempted to invoke non-callable value or invalid arity");
  }

  // IC Ownership: The IC slot owns a reference to the callable object (key).
  retain(currentVal);

  struct {
    RTValue key;
    void *method;
  } __attribute__((aligned(16))) pair;
  pair.key = currentVal;
  pair.method = method;

  typedef __int128_t int128;
  int128 *dest = (int128 *)slot;
  int128 src;
  memcpy(&src, &pair, 16);
  int128 old_val = __atomic_exchange_n(dest, src, __ATOMIC_ACQ_REL);

  struct {
    RTValue key;
    void *method;
  } *old_pair = (void *)&old_val;

  if (old_pair->key != 0) {
    release(old_pair->key);
  }
  return method;
}
