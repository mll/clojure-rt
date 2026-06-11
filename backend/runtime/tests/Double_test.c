#include "TestTools.h"
#include "../Double.h"
#include <cmocka.h>

static void test_double_tostring_normal(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue d = RT_boxDouble(1.234);
    String *s = Double_toString(d);
    assert_string_equal(String_c_str(s), "1.234");
    Ptr_release(s);
  });
}

static void test_double_tostring_whole_number(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue d = RT_boxDouble(42.0);
    String *s = Double_toString(d);
    assert_string_equal(String_c_str(s), "42.0");
    Ptr_release(s);
  });
}

static void test_double_tostring_nan(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue d = RT_boxDouble(__builtin_nan(""));
    String *s = Double_toString(d);
    assert_string_equal(String_c_str(s), "NaN");
    Ptr_release(s);
  });
}

static void test_double_tostring_infinity(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue d_pos = RT_boxDouble(__builtin_inf());
    String *s_pos = Double_toString(d_pos);
    assert_string_equal(String_c_str(s_pos), "Infinity");
    Ptr_release(s_pos);

    RTValue d_neg = RT_boxDouble(-__builtin_inf());
    String *s_neg = Double_toString(d_neg);
    assert_string_equal(String_c_str(s_neg), "-Infinity");
    Ptr_release(s_neg);
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_double_tostring_normal),
      cmocka_unit_test(test_double_tostring_whole_number),
      cmocka_unit_test(test_double_tostring_nan),
      cmocka_unit_test(test_double_tostring_infinity),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
