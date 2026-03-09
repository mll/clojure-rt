#include "TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "../Keyword.h"
#include "../PersistentVector.h"
#include "../RuntimeInterface.h"
#include "../String.h"

static void test_keyword_interning(void **state) {
  (void)state; // unused

  // This test proves that the memory balancing allows unique string allocations
  // introduced by newly interned keywords (which leak into the hash map
  // globally)
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("my-unique-test-key");
    RTValue kw = Keyword_create(s);

    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, kw);

    Ptr_release(v);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_keyword_interning),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
