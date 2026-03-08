#include <iostream>

#include "../RuntimeHeaders.h"

extern "C" {
#include "../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
}

#include "../tools/EdnParser.h"

static void test_edn_parser_trivial_memory(void **state) {
  (void)state; // unused

  ASSERT_MEMORY_ALL_BALANCED({
    RTValue val = RT_boxNil();
    release(val);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_edn_parser_trivial_memory),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
