#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "TestTools.h"

static void test_basic_operations(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *m = PersistentArrayMap_empty();
    assert_non_null(m);
    assert_int_equal(0, m->count);

    RTValue k1 = RT_boxInt32(1);
    RTValue v1 = RT_boxInt32(10);
    PersistentArrayMap *m1 = PersistentArrayMap_assoc(m, k1, v1);

    assert_non_null(m1);
    assert_int_equal(1, m1->count);

    Ptr_retain(m1);
    RTValue lookupv1 = PersistentArrayMap_get(m1, RT_boxInt32(1));
    assert_true(RT_isInt32(lookupv1));
    assert_int_equal(10, RT_unboxInt32(lookupv1));
    release(lookupv1);

    RTValue k2 = RT_boxInt32(2);
    RTValue v2 = RT_boxInt32(20);
    PersistentArrayMap *m2 = PersistentArrayMap_assoc(m1, k2, v2);

    assert_int_equal(2, m2->count);
    Ptr_retain(m2);
    RTValue lookupv2 = PersistentArrayMap_get(m2, RT_boxInt32(2));
    assert_int_equal(20, RT_unboxInt32(lookupv2));
    release(lookupv2);

    PersistentArrayMap *m3 = PersistentArrayMap_dissoc(m2, RT_boxInt32(1));
    assert_int_equal(1, m3->count);

    Ptr_retain(m3);
    RTValue lookupv1_after = PersistentArrayMap_get(m3, RT_boxInt32(1));
    assert_true(RT_isNil(lookupv1_after));

    Ptr_release(m3);
  });
}

static void test_immutability(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *m = PersistentArrayMap_empty();
    // Every implementation has a minimum of 8 elements.
    RTValue keys[8];
    RTValue values[8];
    memcpy(m->keys, keys, sizeof(RTValue) * 8);
    memcpy(m->values, values, sizeof(RTValue) * 8);
    Ptr_retain(m);
    PersistentArrayMap *m1 =
        PersistentArrayMap_assoc(m, RT_boxInt32(1), RT_boxInt32(10));
    assert_int_equal(0, m->count);
    assert_int_equal(1, m1->count);
    // Whatever junk was there before should still be there
    assert_int_equal(0, memcmp(m->keys, keys, sizeof(RTValue) * 8));
    assert_int_equal(0, memcmp(m->values, values, sizeof(RTValue) * 8));

    Ptr_retain(m1);
    PersistentArrayMap *m2 =
        PersistentArrayMap_assoc(m1, RT_boxInt32(2), RT_boxInt32(20));
    assert_int_equal(1, m1->count);
    assert_int_equal(2, m2->count);

    Ptr_retain(m2);
    PersistentArrayMap *m3 = PersistentArrayMap_dissoc(m2, RT_boxInt32(1));
    assert_int_equal(2, m2->count);
    assert_int_equal(1, m3->count);

    Ptr_release(m3);
    Ptr_release(m2);
    Ptr_release(m1);
    Ptr_release(m);
  });
}

static void test_equality_and_hashing(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue k1 = RT_boxInt32(1);
    RTValue v1 = RT_boxInt32(10);
    RTValue k2 = RT_boxInt32(2);
    RTValue v2 = RT_boxInt32(20);

    PersistentArrayMap *m1 = PersistentArrayMap_createMany(2, k1, v1, k2, v2);

    PersistentArrayMap *m2 = PersistentArrayMap_empty();
    m2 = PersistentArrayMap_assoc(m2, RT_boxInt32(1), RT_boxInt32(10));
    m2 = PersistentArrayMap_assoc(m2, RT_boxInt32(2), RT_boxInt32(20));

    // PersistentArrayMap_equals consumes args?
    // Wait, based on the user's fix, it seems NOT to consume them anymore?
    // Actually, usually in this model if it's NOT marked "outside ref system"
    // it consumes. BUT PersistentArrayMap_equals is marked /* outside refcount
    // system */.
    assert_true(PersistentArrayMap_equals(m1, m2));
    assert_int_equal(PersistentArrayMap_hash(m1), PersistentArrayMap_hash(m2));

    Ptr_retain(m1);
    PersistentArrayMap *m3 =
        PersistentArrayMap_assoc(m1, RT_boxInt32(3), RT_boxInt32(30));
    assert_false(PersistentArrayMap_equals(m1, m3));

    Ptr_release(m3);
    Ptr_release(m2);
    Ptr_release(m1);
  });
}

static void test_large_map(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *m = PersistentArrayMap_empty();
    for (int i = 0; i < HASHTABLE_THRESHOLD; i++) {
      m = PersistentArrayMap_assoc(m, RT_boxInt32(i), RT_boxInt32(i * 10));
    }
    assert_int_equal(HASHTABLE_THRESHOLD, m->count);

    for (int i = 0; i < HASHTABLE_THRESHOLD; i++) {
      Ptr_retain(m);
      RTValue v = PersistentArrayMap_get(m, RT_boxInt32(i));
      assert_int_equal(i * 10, RT_unboxInt32(v));
      release(v);
    }

    Ptr_release(m);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_basic_operations),
      cmocka_unit_test(test_immutability),
      cmocka_unit_test(test_equality_and_hashing),
      cmocka_unit_test(test_large_map),
  };
  initialise_memory();
  // Warm up the EMPTY singleton to avoid memory imbalance in tests
  PersistentArrayMap *warmup = PersistentArrayMap_empty();
  Ptr_release(warmup);

  return cmocka_run_group_tests(tests, NULL, NULL);
}
