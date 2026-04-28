#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "TestTools.h"
#include "../BigInteger.h"
#include "../Function.h"
#include "../Integer.h"
#include "../PersistentList.h"

#define CONJ_TO_LOOP_FACTOR 100

/**
 * @brief Test case for conjoining elements to PersistentList and checking
 * performance.
 */
static void listConjunctionAndPerformance(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;

  double conj_time = 0.0;
  TimerContext timer_sum;

  ASSERT_MEMORY_ALL_BALANCED({
    TimerContext timer_conj;
    timerStart(&timer_conj);
    pd();

    PersistentList *l = PersistentList_create(RT_boxNull(), NULL);
    for (size_t i = 0; i < size; i++) {
      RTValue n = RT_boxInt32(i);
      PersistentList *k = PersistentList_conj(l, n);
      l = k;
    }

    timerStop(&timer_conj);
    conj_time = timerGetSeconds(&timer_conj);

    printf("\nPersistentList Conj. Time: %f seconds\n", conj_time);
    pd();

    // --- 3. ASSERTION ON SIZE ---
    assert_int_equal(size, l->count);

    // --- 4. TRAVERSAL AND SUM CHECK ---
    PersistentList *tmp = l;
    int64_t sum = 0;

    timerStart(&timer_sum);
    while (tmp != NULL) {
      if (tmp->first) {
        sum += RT_unboxInt32(tmp->first);
      }
      tmp = tmp->rest;
    }
    timerStop(&timer_sum);

    // Expected sum of 0 to N-1: N * (N - 1) / 2
    uint64_t expected_sum = (uint64_t)size * ((uint64_t)size - 1) / 2;
    assert_int_equal(expected_sum, sum);
    printf("PersistentList Sum Time: %f seconds\n",
           timerGetSeconds(&timer_sum));

    assert_true(conj_time < timerGetSeconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);

    // --- 5. CLEANUP ---
    TimerContext timer_release;
    timerStart(&timer_release);
    Ptr_release(l);
    timerStop(&timer_release);
    printf("PersistentList release: %f seconds\n",
           timerGetSeconds(&timer_release));
    assert_true(timerGetSeconds(&timer_release) <
                timerGetSeconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);
    pd();
  });
}

/**
 * @brief Test case for deep promotion of PersistentList.
 */
static void testListPromotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue s1 = RT_boxPtr(String_create("one"));
    RTValue s2 = RT_boxPtr(String_create("two"));

    PersistentList *l1 = PersistentList_create(s1, NULL);
    PersistentList *l2 = PersistentList_conj(l1, s2);

    // Initially local
    assert_false(Object_getRawRefCount((Object *)l1) & SHARED_BIT);
    assert_false(Object_getRawRefCount((Object *)l2) & SHARED_BIT);
    assert_false(Object_getRawRefCount((Object *)RT_unboxPtr(s1)) & SHARED_BIT);
    assert_false(Object_getRawRefCount((Object *)RT_unboxPtr(s2)) & SHARED_BIT);

    // Promote
    promoteToShared(RT_boxPtr(l2));

    // All should be shared
    assert_true(Object_getRawRefCount((Object *)l1) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)l2) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)RT_unboxPtr(s1)) & SHARED_BIT);
    assert_true(Object_getRawRefCount((Object *)RT_unboxPtr(s2)) & SHARED_BIT);

    Ptr_release(l2);
  });
}

/**
 * @brief Test case for "stop if shared" optimization in list promotion.
 */
static void testListPromotionStop(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue s1 = RT_boxPtr(String_create("one"));
    RTValue s2 = RT_boxPtr(String_create("two"));

    PersistentList *l1 = PersistentList_create(s1, NULL);
    PersistentList *l2 = PersistentList_conj(l1, s2);

    // Promote tail first
    promoteToShared(RT_boxPtr(l1));
    assert_true(Object_getRawRefCount((Object *)l1) & SHARED_BIT);
    assert_false(Object_getRawRefCount((Object *)l2) & SHARED_BIT);

    // Promote head - should stop at l1
    promoteToShared(RT_boxPtr(l2));
    assert_true(Object_getRawRefCount((Object *)l2) & SHARED_BIT);

    Ptr_release(l2);
  });
}

static RTValue MockBigIntAddition(Frame *frame, RTValue a, RTValue b, RTValue a2,
                                   RTValue a3, RTValue a4) {
  BigInteger *valA = (BigInteger *)RT_unboxPtr(a);
  BigInteger *valB = (BigInteger *)RT_unboxPtr(b);
  // retain because BigInteger_add will consume them, 
  // but RT_invokeMethodWithFrame will ALSO release them later.
  Ptr_retain(valA);
  Ptr_retain(valB);
  BigInteger *res = BigInteger_add(valA, valB);
  return RT_boxPtr(res);
}

static RTValue create_bigint_add_fn() {
  ClojureFunction *f = Function_create(1, 2, false);
  Function_fillMethod(f, 0, 0, 2, false, MockBigIntAddition, "add", 0);
  return RT_boxPtr(f);
}

static RTValue MockAddition(Frame *frame, RTValue a, RTValue b, RTValue a2,
                            RTValue a3, RTValue a4) {
  int32_t valA = RT_unboxInt32(a);
  int32_t valB = RT_unboxInt32(b);
  release(a);
  release(b);
  return RT_boxInt32(valA + valB);
}

static RTValue create_mock_add_fn() {
  ClojureFunction *f = Function_create(1, 2, false);
  Function_fillMethod(f, 0, 0, 2, false, MockAddition, "add", 0);
  return RT_boxPtr(f);
}

static void testListReduce(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentList *l = PersistentList_empty();
    int count = 1000;
    for (int i = 0; i < count; i++) {
      l = PersistentList_conj(l, RT_boxPtr(BigInteger_createFromInt(i)));
    }

    // 1. reduce with start value
    Ptr_retain(l);
    RTValue addFn = create_bigint_add_fn();
    RTValue res = PersistentList_reduce(l, addFn, RT_boxPtr(BigInteger_createFromInt(0)));
    
    BigInteger *expected = BigInteger_createFromInt((count * (count - 1)) / 2);
    assert_true(BigInteger_equals((BigInteger *)RT_unboxPtr(res), expected));
    release(res);
    Ptr_release(expected);

    // 2. reduce2 (without start value)
    Ptr_retain(l);
    RTValue addFn2 = create_bigint_add_fn();
    RTValue res2 = PersistentList_reduce2(l, addFn2);
    
    BigInteger *expected2 = BigInteger_createFromInt((count * (count - 1)) / 2);
    assert_true(BigInteger_equals((BigInteger *)RT_unboxPtr(res2), expected2));
    release(res2);
    Ptr_release(expected2);

    Ptr_release(l);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_prestate(testScalingBehavior,
                                listConjunctionAndPerformance),
      cmocka_unit_test(testListPromotion),
      cmocka_unit_test(testListPromotionStop),
      cmocka_unit_test(testListReduce),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
