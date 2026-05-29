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

  PersistentArrayMap *currentMap;

  if (ctx && ctx->bindingsMap != RT_boxNil()) {
    currentMap = RT_unboxPtr(ctx->bindingsMap);
    Ptr_retain(currentMap);
  } else {
    currentMap = PersistentArrayMap_empty();
  }

  RTValue nextMap =
      RT_boxPtr(PersistentArrayMap_assoc(currentMap, v, val));

  ExecutionContext *nextCtx = ExecutionContext_create(nextMap);
  return nextCtx;
}

ExecutionContext *
RT_bind_map(__attribute__((swift_context)) struct ExecutionContext *ctx,
            RTValue bindingsMap) __attribute__((swiftcall)) {
  if (getType(bindingsMap) != persistentArrayMapType) {
    release(bindingsMap);
    throwIllegalArgumentException_C(
        "Second argument to RT_bind_map must be a PersistentArrayMap");
  }

  PersistentArrayMap *newBindings = RT_unboxPtr(bindingsMap);
  PersistentArrayMap *currentMap;

  if (ctx && ctx->bindingsMap != RT_boxNil()) {
    currentMap = RT_unboxPtr(ctx->bindingsMap);
    Ptr_retain(currentMap);
  } else {
    currentMap = PersistentArrayMap_empty();
  }

  for (uword_t i = 0; i < newBindings->count; i++) {
    RTValue v = newBindings->keys[i];
    RTValue val = newBindings->values[i];

    if (getType(v) != varType) {
      Ptr_release(currentMap);
      release(bindingsMap);
      throwIllegalArgumentException_C("Binding keys must be Vars");
    }

    Var *var = RT_unboxPtr(v);
    if (!Var_isDynamic(var)) {
      Ptr_release(currentMap);
      release(bindingsMap);
      throwIllegalStateException_C("Can't dynamically bind non-dynamic Var");
    }

    // assoc consumes its arguments, so we must retain them from the source map
    retain(v);
    retain(val);
    currentMap = PersistentArrayMap_assoc(currentMap, v, val);
  }

  release(bindingsMap);
  ExecutionContext *nextCtx = ExecutionContext_create(RT_boxPtr(currentMap));
  return nextCtx;
}

ExecutionContext *
ExecutionContext_pop(__attribute__((swift_context)) struct ExecutionContext *ctx)
    __attribute__((swiftcall)) {
  Ptr_release(ctx);
  return NULL;
}
