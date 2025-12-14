

// tests/PersistentList_test.c

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#include "cmocka.h"
#include "TestTools.h"

#define TEST_SIZE 10000000
#define CONJ_TO_LOOP_FACTOR 100

/**
 * @brief Test case for conjoining elements to PersistentList and checking performance.
 */
static void list_conjunction_and_performance(void **state) {
    (void)state; // Required CMocka signature
    
    TimerContext timer_conj;
    timer_start(&timer_conj);
    pd();
    
    PersistentList *l = PersistentList_create(NULL, NULL);
    for (size_t i = 0; i < TEST_SIZE; i++) {
        Integer *n = Integer_create(i);
        PersistentList *k = PersistentList_conj(l, n);
        l = k;
    }

    timer_stop(&timer_conj);
    double conj_time = timer_get_seconds(&timer_conj);
    
    printf("\nPersistentList Conj. Time: %f seconds\n", conj_time);
    
    // --- 3. ASSERTION ON SIZE ---
    assert_int_equal(TEST_SIZE, l->count);
    
    // --- 4. TRAVERSAL AND SUM CHECK ---
    TimerContext timer_sum;
    PersistentList *tmp = l;
    int64_t sum = 0;
    
    timer_start(&timer_sum);
    while(tmp != NULL) {
        if(tmp->first) {
             sum += ((Integer *)(tmp->first))->value;
        }
        tmp = tmp->rest;
    }
    timer_stop(&timer_sum);
    
    // Expected sum of 0 to N-1: N * (N - 1) / 2    
    uint64_t expected_sum = (uint64_t)TEST_SIZE * ((uint64_t)TEST_SIZE - 1) / 2;
    assert_int_equal(expected_sum, sum);
    printf("PersistentList Sum Time: %f seconds\n", timer_get_seconds(&timer_sum));

    assert_true(conj_time < timer_get_seconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);
    
    // --- 5. CLEANUP ---
    TimerContext timer_release;    
    timer_start(&timer_release);
    release(l);
    timer_stop(&timer_release);
    printf("PersistentList release: %f seconds\n",
           timer_get_seconds(&timer_release));
    assert_true(timer_get_seconds(&timer_release) < timer_get_seconds(&timer_sum) * CONJ_TO_LOOP_FACTOR);
    pd();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(list_conjunction_and_performance),
    };

    initialise_memory();
    srand(0);
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
