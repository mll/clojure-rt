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
 * @brief Tests high-performance creation using Transients (!).
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

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_prestate(testScalingBehavior, vectorConjNoReuse),
      cmocka_unit_test_prestate(testScalingBehavior, vectorStandardConjAndSum),
      cmocka_unit_test_prestate(testScalingBehavior, vectorTransientConj),
      cmocka_unit_test_prestate(testScalingBehavior,
                                vectorTraversalPerformance),
      cmocka_unit_test_prestate(testScalingBehavior, vectorAssocUpdate),
      cmocka_unit_test_prestate(testScalingBehavior, vectorUpdateC),

  };

  initialise_memory();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
