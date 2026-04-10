#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "TestTools.h"

// Factor used for relative performance assertions
#define PERF_FACTOR 100

/**
 * @brief Test: NO REUSE (Purely Persistent)
 * We manually retain the vector to force the ref count > 1.
 * This forces the implementation to copy nodes (Path Copying).
 */
static void vectorConjNoReuse(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  PersistentVector *v = PersistentVector_create();

  TimerContext timer;
  timerStart(&timer);

  for (size_t i = 0; i < size; i++) {
    RTValue n = RT_boxInt32(i);

    // Force ref count up so 'v' cannot be mutated in-place
    Ptr_retain(v);
    PersistentVector *next = PersistentVector_conj(v, n);

    // Release the old reference and the "forced" retention
    Ptr_release(v);

    v = next;
  }

  timerStop(&timer);
  printf("\nVector Conj (NO REUSE) Time: %fs\n", timerGetSeconds(&timer));

  assert_int_equal(size, v->count);
  Ptr_release(v);
}

/**
 * @brief Tests standard PersistentVector_conj performance and integrity.
 */
static void vectorStandardConjAndSum(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  TimerContext timer;
  timerStart(&timer);

  PersistentVector *v = PersistentVector_create();
  for (size_t i = 0; i < size; i++) {
    RTValue n = RT_boxInt32(i);
    // Persistent way: original is released, new one is returned
    PersistentVector *next = PersistentVector_conj(v, n);
    v = next;
  }

  timerStop(&timer);
  printf("\nVector Standard Conj Time: %fs (Size: %zu)\n",
         timerGetSeconds(&timer), size);

  assert_int_equal(size, v->count);

  // Verify Sum using Iterator
  int64_t sum = 0;
  PersistentVectorIterator it = PersistentVector_iterator(v);
  RTValue obj = PersistentVector_iteratorGet(&it);
  while (!RT_isNull(obj)) {
    sum += RT_unboxInt32(obj);
    obj = PersistentVector_iteratorNext(&it);
  }

  uint64_t expected_sum = (uint64_t)size * ((uint64_t)size - 1) / 2;
  assert_int_equal(expected_sum, sum);

  Ptr_release(v);
}

/**
 * @brief Tests high-performance creation using Transients.
 */
static void vectorTransientConj(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  TimerContext timer;
  timerStart(&timer);

  // Enter transient mode
  PersistentVector *v = PersistentVector_transient(PersistentVector_create());

  for (size_t i = 0; i < size; i++) {
    RTValue n = RT_boxInt32(i);
    // Note: conj_BANG_ handles mutation internally in transient state
    v = PersistentVector_conj_BANG_(v, n);
  }

  // Return to persistent mode
  v = PersistentVector_persistent_BANG_(v);

  timerStop(&timer);
  printf("Vector Transient Conj Time: %fs\n", timerGetSeconds(&timer));

  assert_int_equal(size, v->count);
  Ptr_release(v);
}

/**
 * @brief Compares random access (nth) vs sequential access (iterator).
 */
static void vectorTraversalPerformance(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;
  // Setup: Create a vector
  PersistentVector *v = PersistentVector_transient(PersistentVector_create());
  for (size_t i = 0; i < size; i++) {
    v = PersistentVector_conj_BANG_(v, RT_boxInt32(i));
  }
  v = PersistentVector_persistent_BANG_(v);

  // 1. Test nth (Random Access)
  TimerContext t_nth;
  int64_t sum_nth = 0;
  timerStart(&t_nth);
  for (size_t i = 0; i < v->count; i++) {
    Ptr_retain(v);
    RTValue ob = PersistentVector_nth(v, i);
    sum_nth += RT_unboxInt32(ob);
    release(ob);
  }
  timerStop(&t_nth);

  // 2. Test Iterator (Sequential Access)
  TimerContext t_it;
  int64_t sum_it = 0;
  timerStart(&t_it);
  PersistentVectorIterator it = PersistentVector_iterator(v);
  RTValue obj = PersistentVector_iteratorGet(&it);
  while (!RT_isNull(obj)) {
    assert(getType(obj) == integerType);
    sum_it += RT_unboxInt32(obj);
    obj = PersistentVector_iteratorNext(&it);
  }
  timerStop(&t_it);

  printf("Traversal Performance (Size %zu):\n", size);
  printf("  - nth sum time:    %fs\n", timerGetSeconds(&t_nth));
  printf("  - iterator time:   %fs\n", timerGetSeconds(&t_it));

  assert_int_equal(sum_nth, sum_it);
  Ptr_release(v);
}

/**
 * @brief Tests the PersistentVector_assoc (point updates).
 */
static void vectorAssocUpdate(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  PersistentVector *v = PersistentVector_transient(PersistentVector_create());
  for (size_t i = 0; i < size; i++) {
    v = PersistentVector_conj_BANG_(v, RT_boxInt32(0)); // Start with all zeros
  }
  v = PersistentVector_persistent_BANG_(v);

  TimerContext timer;
  timerStart(&timer);
  for (size_t i = 0; i < size; i++) {
    Ptr_retain(v);
    // Update index i to value 7
    PersistentVector *next = PersistentVector_assoc(v, i, RT_boxInt32(7));
    Ptr_release(v); // Release the old reference
    v = next;
  }
  timerStop(&timer);
  printf("Vector Assoc Update Time: %fs\n", timerGetSeconds(&timer));

  // Check integrity: Sum should be 7 * size
  int64_t sum = 0;
  PersistentVectorIterator it = PersistentVector_iterator(v);
  RTValue obj = PersistentVector_iteratorGet(&it);
  while (!RT_isNull(obj)) {
    sum += RT_unboxInt32(obj);
    obj = PersistentVector_iteratorNext(&it);
  }

  assert_int_equal((int64_t)size * 7, sum);
  Ptr_release(v);
}

/**
 * @brief A comparison with pure C array
 */
static void vectorUpdateC(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  int array[size];

  for (size_t i = 0; i < size; i++) {
    array[i] = 0;
  }

  TimerContext timer;
  timerStart(&timer);
  for (size_t i = 0; i < size; i++) {
    array[i] = 7;
  }
  timerStop(&timer);

  printf("C array Update Time: %fs\n", timerGetSeconds(&timer));

  // Check integrity: Sum should be 7 * size
  int64_t sum = 0;
  for (int i = 0; i < (int)size; i++) {
    sum += array[i];
  }

  assert_int_equal((int64_t)size * 7, sum);
}

static void vectorCreateTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    assert_int_equal(v->count, 0);
    assert_int_equal(v->shift, 0);
    assert_null(v->root);
    assert_non_null(v->tail);
    Ptr_release(v);
  });
}

static void vectorSingleEqualityTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v1 = PersistentVector_create();
    PersistentVector *v2 = PersistentVector_create();
    PersistentVector *v3 = PersistentVector_conj(v1, RT_boxInt32(42));
    // v1 is consumed by conj
    PersistentVector *v4 = PersistentVector_conj(v2, RT_boxInt32(42));
    // v2 is consumed by conj

    assert_true(PersistentVector_equals(v3, v4)); // 1 item same value
    // equals does not consume v3 and v4

    Ptr_release(v3);
    Ptr_release(v4);
  });
}

static void vectorConjAndPopTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v1 = PersistentVector_create();
    PersistentVector *v2 = PersistentVector_conj(v1, RT_boxInt32(10));
    // v1 is consumed by conj

    assert_int_equal(v2->count, 1);

    Ptr_retain(v2);
    PersistentVector *v3 = PersistentVector_pop(v2);
    // v2 is consumed by pop

    assert_int_equal(v3->count, 0);
    assert_int_equal(v2->count, 1);

    Ptr_release(v2);
    Ptr_release(v3);
  });
}

static void vectorNthOutOfBoundsTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v1 = PersistentVector_create();

    // Bounds check on empty vector
    ASSERT_THROWS("IndexOutOfBoundsException", {
      PersistentVector_nth(v1, 0); // consumes v1
    });

    PersistentVector *v2 = PersistentVector_createMany(1, RT_boxInt32(99));

    // Valid access
    Ptr_retain(v2); // nth consumes
    RTValue valid = PersistentVector_nth(v2, 0);
    assert_int_equal(RT_unboxInt32(valid), 99);

    // Bounds check on size 1 vector (index 1 is out of bounds)
    ASSERT_THROWS("IndexOutOfBoundsException", {
      PersistentVector_nth(v2, 1); // consumes v2
    });
  });
}

static void test_vector_bounds_exceptions(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // 1. assoc out of bounds
    PersistentVector *v1 = PersistentVector_create();
    ASSERT_THROWS("IndexOutOfBoundsException",
                  { PersistentVector_assoc(v1, 1, RT_boxInt32(42)); });

    // 2. dynamic_nth with non-integer
    PersistentVector *v2 = PersistentVector_createMany(1, RT_boxInt32(10));
    ASSERT_THROWS("IllegalArgumentException",
                  { PersistentVector_dynamic_nth(v2, RT_boxDouble(1.2)); });

    // 3. pop on empty vector
    PersistentVector *v3 = PersistentVector_create();
    ASSERT_THROWS("IllegalStateException", { PersistentVector_pop(v3); });
  });
}

static void vectorStructuralConjTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 5; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i * 10)); // v consumed each loop
    }

    assert_int_equal(v->count, 5);

    for (int i = 0; i < 5; i++) {
      Ptr_retain(v);
      RTValue item = PersistentVector_nth(v, i);
      assert_int_equal(RT_unboxInt32(item), i * 10);
    }

    Ptr_release(v);
  });
}

static void vectorAssocTest(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 5; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i * 10)); // v consumed
    }

    // Replace element at index 2 (currently 20) with 999
    Ptr_retain(v);
    PersistentVector *v_new =
        PersistentVector_assoc(v, 2, RT_boxInt32(999)); // v consumed

    // Check old vector v is unchanged
    RTValue old_item = PersistentVector_nth(v, 2); // v consumed
    assert_int_equal(RT_unboxInt32(old_item), 20);

    // Check new vector has updated value
    RTValue new_item = PersistentVector_nth(v_new, 2); // v_new consumed
    assert_int_equal(RT_unboxInt32(new_item), 999);
  });
}

static void vectorTransientTest(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    PersistentVector *tv = PersistentVector_transient(v); // v consumed

    for (int i = 0; i < 5; i++) {
      tv = PersistentVector_conj_BANG_(tv, RT_boxInt32(i * 10)); // tv consumed
    }

    tv = PersistentVector_assoc_BANG_(tv, 2, RT_boxInt32(999)); // tv consumed

    PersistentVector *v_final =
        PersistentVector_persistent_BANG_(tv); // tv consumed

    assert_int_equal(v_final->count, 5);

    RTValue item2 = PersistentVector_nth(v_final, 2); // consumes v_final
    assert_int_equal(RT_unboxInt32(item2), 999);
  });
}

static void test_vector_hashing(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v1 = PersistentVector_createMany(
        3, RT_boxInt32(1), RT_boxInt32(2), RT_boxInt32(3));
    PersistentVector *v2 = PersistentVector_createMany(
        3, RT_boxInt32(1), RT_boxInt32(2), RT_boxInt32(3));
    PersistentVector *v3 = PersistentVector_createMany(
        3, RT_boxInt32(1), RT_boxInt32(2), RT_boxInt32(4));

    assert_int_equal(PersistentVector_hash(v1), PersistentVector_hash(v2));
    assert_false(PersistentVector_hash(v1) == PersistentVector_hash(v3));

    Ptr_release(v1);
    Ptr_release(v2);
    Ptr_release(v3);
  });
}

static void test_vector_to_string(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v =
        PersistentVector_createMany(2, RT_boxInt32(1), RT_boxInt32(2));
    String *s = PersistentVector_toString(v); // consumes v
    s = String_compactify(s);
    assert_string_equal("[1 2]", String_c_str(s));
    Ptr_release(s);
  });
}

static void test_vector_pop_bang(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_createMany(
        3, RT_boxInt32(1), RT_boxInt32(2), RT_boxInt32(3));
    PersistentVector *tv = PersistentVector_transient(v);

    tv = PersistentVector_pop_BANG_(tv);
    assert_int_equal(2, tv->count);

    PersistentVector *v_final = PersistentVector_persistent_BANG_(tv);
    assert_int_equal(2, v_final->count);

    RTValue last = PersistentVector_nth(v_final, 1); // consumes v_final
    assert_int_equal(2, RT_unboxInt32(last));
  });
}

static void test_vector_contains(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v =
        PersistentVector_createMany(2, RT_boxInt32(10), RT_boxInt32(20));
    Ptr_retain(v);
    assert_true(PersistentVector_contains(v, RT_boxInt32(10)));
    Ptr_retain(v);
    assert_true(PersistentVector_contains(v, RT_boxInt32(20)));
    // Last call consumes v
    assert_false(PersistentVector_contains(v, RT_boxInt32(30)));
  });
}

static void test_vector_large_structural(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // Test size > 32 to trigger root/shift mechanics
    PersistentVector *v = PersistentVector_create();
    for (int i = 0; i < 100; i++) {
      v = PersistentVector_conj(v, RT_boxInt32(i));
    }
    assert_int_equal(100, v->count);
    assert_true(v->shift > 0);

    for (int i = 0; i < 100; i++) {
      Ptr_retain(v);
      RTValue val = PersistentVector_nth(v, i);
      assert_int_equal(i, RT_unboxInt32(val));
      release(val);
    }

    // Pop back down
    while (v->count > 0) {
      v = PersistentVector_pop(v);
    }
    assert_int_equal(0, v->count);
    Ptr_release(v);
  });
}

static void test_vector_equality_deep(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v1 = PersistentVector_create();
    PersistentVector *v2 = PersistentVector_create();
    for (int i = 0; i < 100; i++) {
      v1 = PersistentVector_conj(v1, RT_boxInt32(i));
      v2 = PersistentVector_conj(v2, RT_boxInt32(i));
    }

    assert_true(PersistentVector_equals(v1, v2));

    PersistentVector *v3 =
        PersistentVector_assoc(v1, 50, RT_boxInt32(999)); // consumes v1
    assert_false(PersistentVector_equals(v3, v2));

    Ptr_release(v2);
    Ptr_release(v3);
  });
}

static void test_vector_promotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // 1. Small vector (tail only)
    RTValue s1 = RT_boxPtr(String_create("s1"));
    PersistentVector *v_small = PersistentVector_createMany(1, s1);

    assert_false(Object_getRawRefCount((Object *)v_small) & SHARED_BIT);
    assert_false(Object_getRawRefCount((Object *)RT_unboxPtr(s1)) & SHARED_BIT);

    promoteToShared(RT_boxPtr(v_small));

    assert_true(Object_getRawRefCount((Object *)v_small) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)v_small->tail) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)RT_unboxPtr(s1)) & SHARED_BIT);

    Ptr_release(v_small);

    // 2. Large vector (root + tail)
    PersistentVector *v_large = PersistentVector_create();
    RTValue elements[100];
    for (int i = 0; i < 100; i++) {
      elements[i] = RT_boxPtr(String_create("elem"));
      v_large = PersistentVector_conj(v_large, elements[i]);
    }

    assert_false(Object_getRawRefCount((Object *)v_large) & SHARED_BIT);
    assert_non_null(v_large->root);

    promoteToShared(RT_boxPtr(v_large));

    assert_true(Object_getRawRefCount((Object *)v_large) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)v_large->tail) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)v_large->root) & SHARED_BIT);

    for (int i = 0; i < 100; i++) {
      assert_true(Object_getRawRefCount((Object *)RT_unboxPtr(elements[i])) &
                  SHARED_BIT);
    }

    Ptr_release(v_large);
  });
}

static void test_vector_transient_promotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_transient(PersistentVector_create());
    v = PersistentVector_conj_BANG_(v, RT_boxInt32(1));

    ASSERT_THROWS("IllegalStateException", { promoteToShared(RT_boxPtr(v)); });

    Ptr_release(v);
  });
}

static void test_object_shared_not_reusable(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    promoteToShared(RT_boxPtr(v));

    // Refcount is 1 (logical) + SHARED_BIT. Logical refcount is 1.
    // In-place mutation should be blocked.
    assert_false(Ptr_isReusable(v));

    Ptr_release(v);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(vectorCreateTest),
      cmocka_unit_test(vectorSingleEqualityTest),
      cmocka_unit_test(vectorConjAndPopTest),
      cmocka_unit_test(vectorNthOutOfBoundsTest),
      cmocka_unit_test(vectorStructuralConjTest),
      cmocka_unit_test(vectorAssocTest),
      cmocka_unit_test(vectorTransientTest),
      cmocka_unit_test(test_vector_bounds_exceptions),
      cmocka_unit_test(test_vector_hashing),
      cmocka_unit_test(test_vector_to_string),
      cmocka_unit_test(test_vector_pop_bang),
      cmocka_unit_test(test_vector_contains),
      cmocka_unit_test(test_vector_large_structural),
      cmocka_unit_test(test_vector_equality_deep),
      cmocka_unit_test(test_vector_promotion),
      cmocka_unit_test(test_vector_transient_promotion),
      cmocka_unit_test(test_object_shared_not_reusable),
      cmocka_unit_test_prestate(testScalingBehavior, vectorConjNoReuse),
      cmocka_unit_test_prestate(testScalingBehavior, vectorStandardConjAndSum),
      cmocka_unit_test_prestate(testScalingBehavior, vectorTransientConj),
      cmocka_unit_test_prestate(testScalingBehavior,
                                vectorTraversalPerformance),
      cmocka_unit_test_prestate(testScalingBehavior, vectorAssocUpdate),
      cmocka_unit_test_prestate(testScalingBehavior, vectorUpdateC),

  };

  initialise_memory();
  RuntimeInterface_initialise();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
