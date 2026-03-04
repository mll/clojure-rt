#ifndef RUNTIME_TESTS
#define RUNTIME_TESTS

#include "../Object.h"
#include "../defines.h"
#include <assert.h>
#include <gmp.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "cmocka.h"

// #include <gperftools/profiler.h>

void pd();

// A structure to hold the start and end clock times
typedef struct {
  clock_t start;
  clock_t end;
} TimerContext;

typedef struct {
  double slope;     // 'a' in y = ax + b
  double intercept; // 'b' in y = ax + b
  double r_squared; // Linearity: 1.0 is perfectly linear
} RegressionResults;

typedef struct {
  int current_size;
  void *user_data;
} TestParams;

typedef struct {
  size_t test_count;
  double *exec_times;
  int *sizes_to_test;
} PerformanceTestState;

// Function declarations
void timerStart(TimerContext *context);
void timerStop(TimerContext *context);
double timerGetSeconds(const TimerContext *context);

void testScalingBehavior(void **state);

#define ASSERT_MEMORY_BALANCE(type, block)                                     \
  do {                                                                         \
    uword_t start_count = allocationCount[(type)-1];                           \
    block uword_t end_count = allocationCount[(type)-1];                       \
    assert_int_equal(start_count, end_count);                                  \
  } while (0)

typedef struct {
  uword_t counts[256];
} MemoryState;

void captureMemoryState(MemoryState *state);
void assertMemoryBalance(MemoryState *before, MemoryState *after);
// `except_types` expects 1-based objectType values (like integerType)
void assertMemoryBalanceExcept(MemoryState *before, MemoryState *after,
                               int except_types[], size_t num_exceptions);
void assertMemoryDifference(MemoryState *before, MemoryState *after, int type,
                            int expected_diff);

// Macro to assert that absolutely no memory balance changes occurred for any
// type
#define ASSERT_MEMORY_ALL_BALANCED(block)                                      \
  do {                                                                         \
    MemoryState __before, __after;                                             \
    captureMemoryState(&__before);                                             \
    {block} captureMemoryState(&__after);                                      \
    assertMemoryBalance(&__before, &__after);                                  \
  } while (0)

#endif
