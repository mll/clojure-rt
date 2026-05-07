#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "TestTools.h"

static void test_bigint_creation(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    BigInteger *bi1 = BigInteger_createFromInt(123456789);
    assert_non_null(bi1);

    String *s1 = BigInteger_toString(bi1); // consumes bi1
    s1 = String_compactify(s1);
    const char *cstr = String_c_str(s1);
    assert_string_equal("123456789N", cstr);
    Ptr_release(s1);

    BigInteger *bi2 =
        BigInteger_createFromStr("123456789012345678901234567890");
    assert_non_null(bi2);
    String *s2 = BigInteger_toString(bi2);
    s2 = String_compactify(s2);
    assert_string_equal("123456789012345678901234567890N", String_c_str(s2));
    Ptr_release(s2);
  });
}

static void test_bigint_arithmetic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    BigInteger *a = BigInteger_createFromInt(100);
    BigInteger *b = BigInteger_createFromInt(200);

    // BigInteger_add consumes a and b
    BigInteger *c = BigInteger_add(a, b);
    assert_non_null(c);

    Ptr_retain(c);
    double d = BigInteger_toDouble(c); // consumes c
    assert_double_equal(300.0, d, 0.0001);
    Ptr_release(c);

    // Test subtraction
    BigInteger *x = BigInteger_createFromInt(500);
    BigInteger *y = BigInteger_createFromInt(200);
    BigInteger *z = BigInteger_sub(x, y);
    Ptr_retain(z);
    assert_double_equal(300.0, BigInteger_toDouble(z), 0.0001);
    Ptr_release(z);

    // Test multiplication
    BigInteger *m1 = BigInteger_createFromInt(10);
    BigInteger *m2 = BigInteger_createFromInt(20);
    BigInteger *m3 = BigInteger_mul(m1, m2);
    Ptr_retain(m3);
    assert_double_equal(200.0, BigInteger_toDouble(m3), 0.0001);
    Ptr_release(m3);

    // Test division (exact)
    BigInteger *d1 = BigInteger_createFromInt(100);
    BigInteger *d2 = BigInteger_createFromInt(10);
    RTValue res = BigInteger_div(d1, d2);
    assert_non_null(res);
    BigInteger *d3 = RT_unboxPtr(res);
    Ptr_retain(d3);
    assert_double_equal(10.0, BigInteger_toDouble(d3), 0.0001);
    Ptr_release(d3);
  });
}

static void test_bigint_comparison(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    BigInteger *a = BigInteger_createFromInt(100);
    BigInteger *b = BigInteger_createFromInt(100);
    BigInteger *c = BigInteger_createFromInt(200);

    assert_true(BigInteger_equals(a, b));
    assert_false(BigInteger_equals(a, c));
    assert_int_equal(BigInteger_hash(a), BigInteger_hash(b));

    Ptr_retain(a);
    Ptr_retain(c);
    assert_true(BigInteger_gte(c, a)); // consumes c and a

    BigInteger *x = BigInteger_createFromInt(50);
    BigInteger *y = BigInteger_createFromInt(100);
    assert_true(BigInteger_lt(x, y)); // consumes x and y

    Ptr_release(a);
    Ptr_release(b);
    Ptr_release(c);
  });
}

static void test_bigint_division_by_zero(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    BigInteger *a = BigInteger_createFromInt(10);
    BigInteger *b = BigInteger_createFromInt(0);
    ASSERT_THROWS("ArithmeticException", { BigInteger_div(a, b); });
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_bigint_creation),
      cmocka_unit_test(test_bigint_arithmetic),
      cmocka_unit_test(test_bigint_comparison),
      cmocka_unit_test(test_bigint_division_by_zero),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  return cmocka_run_group_tests(tests, NULL, NULL);
}
