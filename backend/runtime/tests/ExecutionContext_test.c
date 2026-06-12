#include "TestTools.h"
#include "../ExecutionContext.h"
#include "../Var.h"
#include "../Symbol.h"
#include "../String.h"
#include <cmocka.h>

static void test_execution_context_create(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ExecutionContext *ctx = ExecutionContext_create(RT_boxNil());
    assert_non_null(ctx);
    assert_true(ctx->bindingsMap == RT_boxNil());
    Ptr_release(ctx);
  });
}

static void test_execution_context_clone(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ExecutionContext *ctx = ExecutionContext_create(RT_boxNil());
    ExecutionContext *cloned = ExecutionContext_clone(ctx); // clone consumes ctx reference
    assert_non_null(cloned);
    assert_true(cloned->bindingsMap == RT_boxNil());
    Ptr_release(cloned);
  });
}

static void test_execution_context_bind(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createDynamicStr("test1");
    Symbol *sym1 = Symbol_create(s1);
    // Setup a dynamic var
    Var *v = Var_create(sym1);
    v->dynamic = true;
    RTValue varVal = RT_boxPtr(v);

    RTValue valToBind = RT_boxDouble(42.0);

    // Initial context is usually NULL or simple
    ExecutionContext *ctx = NULL;

    // Bind
    ExecutionContext *newCtx = RT_bind(ctx, varVal, valToBind);
    assert_non_null(newCtx);
    
    // Test that the binding map is not nil
    assert_true(newCtx->bindingsMap != RT_boxNil());
    
    // Pop
    ExecutionContext *popped = ExecutionContext_pop(newCtx);
    (void)popped;
    // popped releases newCtx, which releases its map, which releases v and valToBind
  });
}

static void test_execution_context_bind_errors(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue nonVar = RT_boxDouble(1.0);
    ASSERT_THROWS("IllegalArgumentException", {
      RT_bind(NULL, nonVar, RT_boxNil());
    });

    String *s2 = String_createDynamicStr("test2");
    Symbol *sym2 = Symbol_create(s2);
    Var *nonDynamicVar = Var_create(sym2);
    nonDynamicVar->dynamic = false;
    RTValue varVal = RT_boxPtr(nonDynamicVar);

    ASSERT_THROWS("IllegalStateException", {
      RT_bind(NULL, varVal, RT_boxNil());
    });
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_execution_context_create),
      cmocka_unit_test(test_execution_context_clone),
      cmocka_unit_test(test_execution_context_bind),
      cmocka_unit_test(test_execution_context_bind_errors),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
