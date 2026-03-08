#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "TestTools.h"

static void test_ratio_creation(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Ratio *r1 = Ratio_createFromInts(1, 2);
    assert_non_null(r1);

    Ptr_retain(r1);
    assert_double_equal(0.5, Ratio_toDouble(r1), 0.0001);

    Ptr_retain(r1);
    String *s1 = Ratio_toString(r1);
    s1 = String_compactify(s1);
    assert_string_equal("1/2", String_c_str(s1));
    Ptr_release(s1);

    Ptr_release(r1);
  });
}

static void test_ratio_arithmetic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Ratio *r1 = Ratio_createFromInts(1, 4);
    Ratio *r2 = Ratio_createFromInts(1, 4);

    // 1/4 + 1/4 = 1/2
    void *res = Ratio_add(r1, r2);
    assert_non_null(res);
    Ratio *r3 = (Ratio *)res;
    assert_int_equal(ratioType, getType(RT_boxPtr(r3)));

    Ptr_retain(r3);
    assert_double_equal(0.5, Ratio_toDouble(r3), 0.0001);

    // 1/2 * 2 = 1 (BigInteger)
    Ratio *two = Ratio_createFromInt(2);
    void *res2 = Ratio_mul(r3, two);
    assert_non_null(res2);
    assert_int_equal(bigIntegerType, getType(RT_boxPtr(res2)));

    Ptr_release(res2);
  });
}

static void test_ratio_simplify(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Ratio *r = Ratio_createFromInts(4, 2);

    void *simplified = Ratio_simplify(r);
    assert_int_equal(bigIntegerType, getType(RT_boxPtr(simplified)));
    Ptr_release(simplified);
  });
}

static void test_ratio_comparison(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Ratio *r1 = Ratio_createFromInts(1, 3);
    Ratio *r2 = Ratio_createFromInts(2, 6);
    Ratio *r3 = Ratio_createFromInts(1, 2);

    assert_true(Ratio_equals(r1, r2));
    assert_int_equal(Ratio_hash(r1), Ratio_hash(r2));

    Ptr_retain(r1);
    Ptr_retain(r3);
    assert_true(Ratio_lt(r1, r3)); // consumes r1 and r3

    Ptr_release(r1);
    Ptr_release(r2);
    Ptr_release(r3);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_ratio_creation),
      cmocka_unit_test(test_ratio_arithmetic),
      cmocka_unit_test(test_ratio_simplify),
      cmocka_unit_test(test_ratio_comparison),
  };
  initialise_memory();
  return cmocka_run_group_tests(tests, NULL, NULL);
}
