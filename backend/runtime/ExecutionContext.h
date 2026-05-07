#ifndef EXECUTION_CONTEXT_H
#define EXECUTION_CONTEXT_H

#include "ObjectProto.h"
#include "RTValue.h"
#include "String.h"

typedef struct ExecutionContext {
  Object super;
  RTValue bindingsMap; // PersistentArrayMap
} ExecutionContext;

ExecutionContext *ExecutionContext_create(RTValue bindingsMap);
void ExecutionContext_destroy(ExecutionContext *self);
ExecutionContext *ExecutionContext_clone(ExecutionContext *self);

ExecutionContext *
RT_bind(__attribute__((swift_context)) struct ExecutionContext *ctx, RTValue v,
        RTValue val) __attribute__((swiftcall));

ExecutionContext *
RT_bind_map(__attribute__((swift_context)) struct ExecutionContext *ctx,
            RTValue bindingsMap) __attribute__((swiftcall));

#endif
