#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "TestTools.h"

static void test_boolean_to_string(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue trueVal = RT_boxBool(true);
    RTValue falseVal = RT_boxBool(false);

    String *s1 = Boolean_toString(trueVal);
    assert_string_equal("true", String_c_str(s1));
    Ptr_release(s1);

    String *s2 = Boolean_toString(falseVal);
    assert_string_equal("false", String_c_str(s2));
    Ptr_release(s2);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_boolean_to_string),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  return cmocka_run_group_tests(tests, NULL, NULL);
}
