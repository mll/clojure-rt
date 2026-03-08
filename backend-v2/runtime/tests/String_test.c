#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "../String.h"
#include "TestTools.h"

static void testStaticStringMemory(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("hello");
    String *s2 =
        String_createStaticOptimised("hello", 5, String_computeHash("hello"));

    // Assert basic properties outside ref system
    assert_true(String_equals(s1, s2));
    assert_int_equal(s1->count, 5);

    String *s3 = String_createStatic("ell");
    assert_int_equal(String_indexOf(s1, s3), 1);

    Ptr_release(s2);
  });
}

static void testDynamicStringMemory(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createDynamicStr("world");
    String *s2 = String_createDynamicStr("world");

    assert_true(String_equals(s1, s2));

    // Test iterators (outside refsystem)
    StringIterator it = String_iterator(s1);
    assert_int_equal(*String_iteratorGet(&it), 'w');
    assert_int_equal(*String_iteratorNext(&it), 'o');
    assert_int_equal(*String_iteratorNext(&it), 'r');

    // Test replacement (compacts internally but handles dynamically)
    String *target = String_createStatic("r");
    String *replacement = String_createStatic("x");

    // String_replace consumes the references to target and replacement, returns
    // new string natively retaining count of 1.
    Ptr_retain(s1);
    String *s3 = String_replace(s1, target, replacement); // "woxld"
    assert_false(String_equals(s1, s3));

    // s1 is untouched.
    Ptr_release(s1);
    Ptr_release(s2);
    Ptr_release(s3);
  });
}

static void testCompoundStringMemory(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("hello ");
    String *s2 = String_createDynamicStr("world");

    String *compound = String_concat(s1, s2);

    assert_int_equal(compound->count, 11);
    assert_true(compound->specialisation == compoundString);

    // Iterating over compound string checks iterator boundary logic without ref
    // counts
    StringIterator it = String_iterator(compound);
    assert_int_equal(*String_iteratorGet(&it), 'h');
    for (int i = 0; i < 6; i++)
      String_iteratorNext(&it);
    assert_int_equal(*String_iteratorGet(&it), 'w');

    Ptr_release(compound);
  });
}

static void testStringCompactify(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("foo");
    String *s2 = String_createStatic("bar");
    String *s3 = String_createStatic("baz");

    String *c1 = String_concat(s1, s2); // "foobar"
    String *c2 = String_concat(c1, s3); // "foobarbaz"

    // Compactify compound string: This consumes c2!
    String *compacted = String_compactify(c2);

    assert_true(compacted->specialisation == dynamicString);
    assert_int_equal(compacted->count, 9);

    Ptr_release(compacted);
  });
}

static void testStringSearch(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_createStatic("hello world");
    String *target = String_createStatic("world");

    // String_indexOfFrom and String_contains consume their arguments.
    Ptr_retain(s);
    Ptr_retain(target);
    assert_int_equal(String_indexOfFrom(s, target, 0), 6);

    Ptr_retain(s);
    Ptr_retain(target);
    assert_true(String_contains(s, target));

    // Test not found
    Ptr_retain(s);
    String *missing = String_createStatic("missing");
    assert_int_equal(String_indexOf(s, missing), -1);

    // cleanup is handled by consumption, but we need to release 'target' if we
    // didn't use it in last call
    Ptr_release(target);
    Ptr_release(s);
  });
}

static void testStringHashUpdate(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_createDynamicStr("temp");
    uword_t oldHash = String_hash(s);

    // Simulate mutation if we were to allow it (recomputeHash expects
    // non-compound)
    char *dyn =
        &(s->value[0]); // Using implementation detail since it's a runtime test
    dyn[0] = 'z';
    String_recomputeHash(s);

    assert_false(String_hash(s) == oldHash);
    assert_int_equal(String_hash(s), String_computeHash("zemp"));

    Ptr_release(s);
  });
}

static void testStringEdgeCases(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *empty = String_createStatic("");
    String *s = String_createStatic("content");

    Ptr_retain(empty);
    Ptr_retain(s);
    assert_int_equal(String_indexOf(s, empty), 0);

    Ptr_retain(empty);
    Ptr_retain(s);
    assert_int_equal(String_indexOf(empty, s), -1);

    // Compound chain
    String *c = String_createStatic("");
    for (int i = 0; i < 10; i++) {
      c = String_concat(c, String_createStatic("a"));
    }
    assert_int_equal(c->count, 10);
    String *compact = String_compactify(c);
    assert_string_equal("aaaaaaaaaa", String_c_str(compact));
    Ptr_release(compact);
    Ptr_release(s);
    Ptr_release(empty);
  });
}

static void testStringMemoryManagement(void **state) {
  (void)state;
  // This test explicitly checks if memory balances when expecting consumption
  MemoryState before, after;
  captureMemoryState(&before);

  String *s = String_createDynamicStr("long string to be searched");
  String *sub = String_createDynamicStr("searched");

  // String_contains(s, sub) should consume both
  assert_true(String_contains(s, sub));

  captureMemoryState(&after);
  assertMemoryBalance(&before, &after);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(testStaticStringMemory),
      cmocka_unit_test(testDynamicStringMemory),
      cmocka_unit_test(testCompoundStringMemory),
      cmocka_unit_test(testStringCompactify),
      cmocka_unit_test(testStringSearch),
      cmocka_unit_test(testStringHashUpdate),
      cmocka_unit_test(testStringEdgeCases),
      cmocka_unit_test(testStringMemoryManagement),
  };
  initialise_memory();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
