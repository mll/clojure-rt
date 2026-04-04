#include "../Exception.h"
#include "../Object.h"
#include "../String.h"
#include "TestTools.h"
#include <cmocka.h>

static void test_exception_creation(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *msg = String_createStatic("Test error message");
    Exception *exc = Exception_createAssertionErrorWithMessage(msg);

    assert_non_null(exc);
    assert_int_equal(exceptionType, getType(RT_boxPtr(exc)));
    assert_non_null(exc->bridgedData);

    // Test equals and hash
    assert_true(equals(RT_boxPtr(exc), RT_boxPtr(exc)));
    assert_int_equal(Exception_hash(exc), hash(RT_boxPtr(exc)));

    // Test toString
    String *excStr = toString(RT_boxPtr(exc));
    assert_non_null(excStr);
    assert_string_equal("Exception", String_c_str(excStr));
    release(RT_boxPtr(excStr));
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exception_creation),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
