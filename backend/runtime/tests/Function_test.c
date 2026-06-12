#include "TestTools.h"
#include "../Function.h"
#include <cmocka.h>

static void test_function_create_destroy(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ClojureFunction *f = Function_create(1, 2, false);
    assert_non_null(f);
    assert_int_equal(f->methodCount, 1);
    assert_int_equal(f->maxArity, 2);
    assert_false(f->once);
    Ptr_release(f);
  });
}

static void test_function_fill_method_and_valid_call(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ClojureFunction *f = Function_create(2, 2, false);
    
    // Fill method 0: 2 args fixed
    Function_fillMethod(f, 0, 0, 2, false, NULL, NULL, 0);
    
    // Fill method 1: 3 args variadic (meaning >= 3 args)
    Function_fillMethod(f, 1, 1, 3, true, NULL, NULL, 1, RT_boxDouble(1.0));
    
    assert_false(Function_validCallWithArgCount(f, 1));
    assert_true(Function_validCallWithArgCount(f, 2));
    assert_true(Function_validCallWithArgCount(f, 3));
    assert_true(Function_validCallWithArgCount(f, 4)); // hits variadic

    Ptr_release(f);
  });
}

static void test_function_tostring(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ClojureFunction *f = Function_create(1, 1, false);
    Function_fillMethod(f, 0, 0, 1, false, NULL, NULL, 0);
    
    // Retain to avoid DoubleFree if toString consumes
    Ptr_retain(f);
    String *s = Function_toString(f);
    assert_non_null(s);
    Ptr_release(s);
    Ptr_release(f);
  });
}

static void test_function_cleanup_once(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ClojureFunction *f = Function_create(1, 1, true); // once = true
    Function_fillMethod(f, 0, 0, 1, false, NULL, NULL, 1, RT_boxDouble(1.0));
    
    Function_cleanupOnce(f);
    assert_true(f->executed);

    // double cleanupOnce should assert but we can't test assert easily here,
    // destroying it now shouldn't double free the closed overs since executed is true.
    Ptr_release(f);
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_function_create_destroy),
      cmocka_unit_test(test_function_fill_method_and_valid_call),
      cmocka_unit_test(test_function_tostring),
      cmocka_unit_test(test_function_cleanup_once),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
