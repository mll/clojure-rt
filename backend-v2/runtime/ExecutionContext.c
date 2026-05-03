#include "ExecutionContext.h"
#include "Exceptions.h"
#include "Object.h"
#include "PersistentArrayMap.h"
#include "RTValue.h"
#include "Var.h"

ExecutionContext *ExecutionContext_create(RTValue bindingsMap) {
  ExecutionContext *self =
      (ExecutionContext *)allocate(sizeof(ExecutionContext));
  Object_create(&self->super, executionContextType);
  self->bindingsMap = bindingsMap;
  return self;
}

void ExecutionContext_destroy(ExecutionContext *self) {
  release(self->bindingsMap);
}

ExecutionContext *ExecutionContext_clone(ExecutionContext *self) {
  retain(self->bindingsMap);
  Ptr_release(self);
  return ExecutionContext_create(self->bindingsMap);
}

ExecutionContext *
RT_bind(__attribute__((swift_context)) struct ExecutionContext *ctx, RTValue v,
        RTValue val) __attribute__((swiftcall)) {
  if (getType(v) != varType) {
    release(v);
    release(val);
    throwIllegalArgumentException_C("First argument to RT_bind must be a Var");
  }

  Var *var = RT_unboxPtr(v);
  if (!Var_isDynamic(var)) {
    release(v);
    release(val);
    throwIllegalStateException_C("Can't dynamically bind non-dynamic Var");
  }

  RTValue currentMap = (ctx && ctx->bindingsMap != RT_boxNil())
                           ? ctx->bindingsMap
                           : RT_boxPtr(PersistentArrayMap_empty());
  RTValue nextMap = RT_boxPtr(
      PersistentArrayMap_assoc(RT_unboxPtr(currentMap), var->keyword, val));

  ExecutionContext *nextCtx = ExecutionContext_create(nextMap);
  return nextCtx;
}
