#include "TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "../PersistentVector.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Symbol.h"

static void test_symbol_interning(void **state) {
  (void)state; // unused

  // This test proves that the memory balancing allows unique string allocations
  // introduced by newly interned symbols (which leak into the hash map
  // globally)
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("my-unique-test-symbol");
    RTValue sym = Symbol_create(s);

    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, sym);

    Ptr_release(v);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_symbol_interning),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
