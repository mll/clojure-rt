#include "TestTools.h"
#include <cmocka.h>
#include <stdio.h>

static void test_integer_div_basic(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue res = Integer_div(10, 2);
    assert_true(RT_isInt32(res));
    assert_int_equal(RT_unboxInt32(res), 5);

    res = Integer_div(10, 3);
    assert_true(getType(res) == ratioType);
    Ptr_release(RT_unboxPtr(res));

    res = Integer_div(0, 5);
    assert_true(RT_isInt32(res));
    assert_int_equal(RT_unboxInt32(res), 0);
  });
}

static void test_integer_division_by_zero(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    ASSERT_THROWS("ArithmeticException", {
      Integer_div(10, 0);
    });
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_integer_div_basic),
      cmocka_unit_test(test_integer_division_by_zero),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
