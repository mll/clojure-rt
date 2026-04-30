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
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("my-unique-test-symbol");
    RTValue sym = Symbol_create(s);

    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, sym);

    Ptr_release(v);
  });
}

static void test_symbol_metadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *name = String_create("meta-sym");
    RTValue meta = RT_boxPtr(PersistentArrayMap_empty());
    meta = RT_boxPtr(PersistentArrayMap_assoc(RT_unboxPtr(meta), Keyword_create(String_create("tag")), RT_boxPtr(String_create("special"))));

    // Create symbol with meta
    RTValue sym = Symbol_createWithMeta(name, meta);
    
    // In our implementation, symbols are always equal by name
    String *name2 = String_create("meta-sym");
    RTValue sym2 = Symbol_create(name2);

    assert_true(Object_equals((Object *)RT_unboxSymbol(sym), (Object *)RT_unboxSymbol(sym2)));
    // But they are not the same object
    assert_ptr_not_equal(RT_unboxSymbol(sym), RT_unboxSymbol(sym2));

    release(sym);
    release(sym2);
  });
}

static void test_symbol_equality(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // Interned symbols should be identical
    RTValue s1 = Symbol_create(String_create("foo"));
    RTValue s2 = Symbol_create(String_create("foo"));
    assert_ptr_equal(RT_unboxSymbol(s1), RT_unboxSymbol(s2));

    // Symbol with meta should not be the interned one
    RTValue s3 = Symbol_createWithMeta(String_create("foo"), RT_boxNil());
    assert_ptr_not_equal(RT_unboxSymbol(s1), RT_unboxSymbol(s3));
    assert_true(Object_equals((Object *)RT_unboxSymbol(s1), (Object *)RT_unboxSymbol(s3)));

    release(s1);
    release(s2);
    release(s3);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_symbol_interning),
      cmocka_unit_test(test_symbol_metadata),
      cmocka_unit_test(test_symbol_equality),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
