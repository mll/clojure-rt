#include "../ExecutionContext.h"
#include "../Keyword.h"
#include "../PersistentArrayMap.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Var.h"
#include "TestTools.h"
#include "runtime/BigInteger.h"
#include "runtime/Ebr.h"
#include "runtime/RTValue.h"
#include <cmocka.h>

static void test_dynamic_var_binding(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("dynamic-var"));
    retain(sym);
    Var *v = Var_create(sym);

    Var_setDynamic(v, true);
    Ptr_retain(v);
    Var_bindRoot(v, RT_boxPtr(BigInteger_createFromInt(1)));

    // ctx = {v: 10}
    // Note: PersistentArrayMap_createMany expects pairCount then key, value,
    // ...
    PersistentArrayMap *m = PersistentArrayMap_createMany(
        1, sym, RT_boxPtr(BigInteger_createFromInt(10)));
    ExecutionContext *ctx = ExecutionContext_create(RT_boxPtr(m));

    // 1. Deref without context
    Ptr_retain(v);
    RTValue res1 = Var_deref(NULL, v);
    BigInteger *t1 = BigInteger_createFromInt(1);
    assert_true(equals(RT_boxPtr(t1), res1));
    Ptr_release(t1);
    release(res1);

    // 2. Deref with context
    Ptr_retain(v);
    RTValue res2 = Var_deref(ctx, v);
    BigInteger *t2 = BigInteger_createFromInt(10);
    assert_true(equals(RT_boxPtr(t2), res2));
    Ptr_release(t2);
    release(res2);

    // 3. Deref without context again
    Ptr_retain(v);
    RTValue res3 = Var_deref(NULL, v);
    BigInteger *t3 = BigInteger_createFromInt(1);
    assert_true(equals(RT_boxPtr(t3), res3));
    Ptr_release(t3);
    release(res3);

    release(RT_boxPtr(ctx));
    Ptr_release(v);
    Ebr_force_reclaim();
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_dynamic_var_binding),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
