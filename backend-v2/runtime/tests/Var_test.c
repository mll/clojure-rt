#include "TestTools.h"
#include <cmocka.h>

#include "../Keyword.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Var.h"
#include "../Ebr.h"

static void test_var_basic_lifecycle(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("my-var"));
    Var *v = Var_create(sym);

    Ptr_retain(v);
    assert_true(Var_isDynamic(v) == false);

    Ptr_retain(v);
    assert_true(Var_hasRoot(v) == false);

    RTValue val = RT_boxPtr(String_create("root-value"));
    Ptr_retain(v);
    Var_bindRoot(v, val);

    Ptr_retain(v);
    assert_true(Var_hasRoot(v) == true);

    Ptr_retain(v);
    RTValue deref = Var_deref(v);
    assert_string_equal(String_c_str(RT_unboxPtr(deref)), "root-value");

    release(deref);

    Ptr_retain(v);
    Var_unbindRoot(v);

    assert_true(Var_hasRoot(v) == false);
    Ebr_synchronize_and_reclaim();
    Ebr_synchronize_and_reclaim();
  });
}

static void test_var_dynamic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("dynamic-var"));
    Var *v = Var_create(sym);

    v = Var_setDynamic(v, true);
    Ptr_retain(v);
    assert_true(Var_isDynamic(v) == true);

    v = Var_setDynamic(v, false);
    Ptr_retain(v);
    assert_true(Var_isDynamic(v) == false);

    Ptr_release(v);
    release(sym);
  });
}

static void test_var_tostring(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("foo"));
    Var *v = Var_create(sym);

    // Var_toString consumes self and may return a compound string
    String *s = Var_toString(v);
    s = String_compactify(s);
    assert_string_equal(String_c_str(s), "#'foo");

    Ptr_release(s);
    release(sym);
  });
}

static void test_var_peek(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("peek-var"));
    Var *v = Var_create(sym);

    RTValue val = RT_boxPtr(String_create("peek-value"));
    // val handle count = 1

    Ptr_retain(v);
    Var_bindRoot(v, val);
    // Var_bindRoot consumes val. val handle count = 1 (owned by Var root)

    Object *obj = (Object *)RT_unboxPtr(val);

    // Test Var_peek
    Ptr_retain(v);
    RTValue peeked = Var_peek(v);
    assert_true(peeked == val);
    // Peek should NOT increment refcount. Should still be 1.
    assert_int_equal(
        atomic_load_explicit(&obj->atomicRefCount, memory_order_relaxed) >> 1,
        1);

    // Test Var_deref for contrast
    Ptr_retain(v);
    RTValue derefed = Var_deref(v);
    assert_true(derefed == val);
    // Deref SHOULD increment refcount. Should be 2 (Var root, derefed handle)
    assert_int_equal(
        atomic_load_explicit(&obj->atomicRefCount, memory_order_relaxed) >> 1,
        2);

    release(derefed);
    // Back to 1.
    assert_int_equal(
        atomic_load_explicit(&obj->atomicRefCount, memory_order_relaxed) >> 1,
        1);

    Ptr_release(v);
    // val is now released because v was destroyed.
    release(sym);
    Ebr_synchronize_and_reclaim();
    Ebr_synchronize_and_reclaim();
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_basic_lifecycle),
      cmocka_unit_test(test_var_dynamic),
      cmocka_unit_test(test_var_tostring),
      cmocka_unit_test(test_var_peek),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
