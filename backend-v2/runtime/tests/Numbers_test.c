#include "TestTools.h"
#include "../Numbers.h"
#include "../Object.h"
#include <cmocka.h>
#include <stdio.h>

static void test_numbers_add_basic(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // Int + Int
    RTValue res = Numbers_add(RT_boxInt32(10), RT_boxInt32(20));
    assert_true(RT_isInt32(res));
    assert_int_equal(RT_unboxInt32(res), 30);

    // Int + Double
    res = Numbers_add(RT_boxInt32(10), RT_boxDouble(2.5));
    assert_true(RT_isDouble(res));
    assert_double_equal(RT_unboxDouble(res), 12.5, 0.0001);

    // Int + BigInt (Overflow)
    res = Numbers_add(RT_boxInt32(2000000000), RT_boxInt32(2000000000));
    assert_true(getType(res) == bigIntegerType);
    Ptr_release(RT_unboxPtr(res));
  });
}

static void test_numbers_div_promotion(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // Int / Int -> Ratio
    RTValue res = Numbers_div(RT_boxInt32(10), RT_boxInt32(3));
    assert_true(getType(res) == ratioType);
    Ptr_release(RT_unboxPtr(res));

    // Int / Int -> Int (if divisible)
    res = Numbers_div(RT_boxInt32(10), RT_boxInt32(2));
    assert_true(RT_isInt32(res));
    assert_int_equal(RT_unboxInt32(res), 5);
  });
}

static void test_numbers_cmp_cross_type(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // Int < Double
    assert_true(Numbers_lt(RT_boxInt32(10), RT_boxDouble(10.5)));
    // Double < Int
    assert_false(Numbers_lt(RT_boxDouble(10.5), RT_boxInt32(10)));
    
    // Int == Double
    assert_true(Numbers_equiv(RT_boxInt32(10), RT_boxDouble(10.0)));
  });
}

static void test_numbers_invalid_args(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue s = RT_boxPtr(String_create("not a number"));
    ASSERT_THROWS("IllegalArgumentException", {
      Numbers_add(RT_boxInt32(10), s);
    });
    
    ASSERT_THROWS("IllegalArgumentException", {
      Numbers_lt(RT_boxInt32(10), RT_boxNil());
    });
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_numbers_add_basic),
      cmocka_unit_test(test_numbers_div_promotion),
      cmocka_unit_test(test_numbers_cmp_cross_type),
      cmocka_unit_test(test_numbers_invalid_args),
  };

  int res = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return res;
}
