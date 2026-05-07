#include "../Numbers.h"
#include "../Object.h"
#include "../ObjectProto.h"
#include "TestTools.h"
#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

    // Int + Int (Overflow)
    ASSERT_THROWS("ArithmeticException", {
      Numbers_add(RT_boxInt32(2000000000), RT_boxInt32(2000000000));
    });
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
    ASSERT_THROWS("IllegalArgumentException",
                  { Numbers_add(RT_boxInt32(10), s); });

    ASSERT_THROWS("IllegalArgumentException",
                  { Numbers_lt(RT_boxInt32(10), RT_boxNil()); });
  });
}

static void test_numbers_nan_tagging(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // Standard NaN (0.0/0.0)
    RTValue qnan = RT_boxDouble(0.0 / 0.0);
    assert_true(RT_isDouble(qnan));
    assert_true(getType(qnan) == doubleType);
    // Should use the canonical Positive Quiet NaN pattern
    assert_int_equal(qnan, 0x7FF8000000000000ULL);

    // Negative NaN (forced bit pattern, should be canonicalized on boxing)
    double neg_nan_raw;
    uint64_t neg_nan_bits = 0xFFF8000000000000ULL;
    memcpy(&neg_nan_raw, &neg_nan_bits, 8);
    RTValue neg_nan = RT_boxDouble(neg_nan_raw);
    assert_true(RT_isDouble(neg_nan));
    assert_int_equal(neg_nan, 0x7FF8000000000000ULL);

    // Negative Infinity (-1.0/0.0) - should be safe after boundary shift
    RTValue neg_inf = RT_boxDouble(-1.0 / 0.0);
    assert_true(RT_isDouble(neg_inf));
    assert_int_equal(neg_inf, 0xFFF0000000000000ULL);

    // Positive Infinity (1.0/0.0)
    RTValue pos_inf = RT_boxDouble(1.0 / 0.0);
    assert_true(RT_isDouble(pos_inf));
    assert_int_equal(pos_inf, 0x7FF0000000000000ULL);

    // Identity of NaNs via equals()
    assert_true(equals(qnan, neg_nan));
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_numbers_add_basic),
      cmocka_unit_test(test_numbers_div_promotion),
      cmocka_unit_test(test_numbers_cmp_cross_type),
      cmocka_unit_test(test_numbers_invalid_args),
      cmocka_unit_test(test_numbers_nan_tagging),
  };

  int res = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return res;
}
