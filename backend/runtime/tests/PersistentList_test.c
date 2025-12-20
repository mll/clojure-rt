#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#include "TestTools.h"

#define CONJ_TO_LOOP_FACTOR 100

/**
 * @brief Test case for conjoining elements to PersistentList and checking performance.
 */
static void listConjunctionAndPerformance(void **state) {
  TestParams *params = (TestParams *)*state;
  size_t size = params->current_size;
  
  TimerContext timer_conj;
  timerStart(&timer_conj);
  pd();
  
  PersistentList *l = PersistentList_create(NULL, NULL);
  for (size_t i = 0; i < size; i++) {
    Integer *n = Integer_create(i);
    PersistentList *k = PersistentList_conj(l, n);
    l = k;
  }
  
  timerStop(&timer_conj);
  double conj_time = timerGetSeconds(&timer_conj);
  
  printf("\nPersistentList Conj. Time: %f seconds\n", conj_time);
  pd();
  
  // --- 3. ASSERTION ON SIZE ---
  assert_int_equal(size, l->count);
  
  // --- 4. TRAVERSAL AND SUM CHECK ---
  TimerContext timer_sum;
  PersistentList *tmp = l;
  int64_t sum = 0;
  
  timerStart(&timer_sum);
  while(tmp != NULL) {
    if(tmp->first) {
      sum += ((Integer *)(tmp->first))->value;
    }
    tmp = tmp->rest;
  }
  timerStop(&timer_sum);
  
  // Expected sum of 0 to N-1: N * (N - 1) / 2    
  uint64_t expected_sum = (uint64_t)size * ((uint64_t)size - 1) / 2;
  assert_int_equal(expected_sum, sum);
  printf("PersistentList Sum Time: %f seconds\n", timerGetSeconds(&timer_sum));
  
  assert_true(conj_time < timerGetSeconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);
  
  // --- 5. CLEANUP ---
  TimerContext timer_release;    
  timerStart(&timer_release);
  release(l);
  timerStop(&timer_release);
  printf("PersistentList release: %f seconds\n",
         timerGetSeconds(&timer_release));
  assert_true(timerGetSeconds(&timer_release) < timerGetSeconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);
  pd();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_prestate(testScalingBehavior, listConjunctionAndPerformance),
    };
    initialise_memory();
    return cmocka_run_group_tests(tests, NULL, NULL);
}
