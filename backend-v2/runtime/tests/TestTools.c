#include "TestTools.h"

// #include <gperftools/profiler.h>

void pd() {
  printf("Ref counters: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
         allocationCount[0], allocationCount[1], allocationCount[2],
         allocationCount[3], allocationCount[4], allocationCount[5],
         allocationCount[6], allocationCount[7], allocationCount[8],
         allocationCount[9], allocationCount[10], allocationCount[11],
         allocationCount[12]);
}

void timerStart(TimerContext *context) {
  if (context) {
    context->start = clock();
  }
}

void timerStop(TimerContext *context) {
  if (context) {
    context->end = clock();
  }
}

double timerGetSeconds(const TimerContext *context) {
  assert(context);
  return (double)(context->end - context->start) / CLOCKS_PER_SEC;
}

/* struct UnitTestGroup perfromanceTestGroup(int repeats, int maxSize, void
 * (*test)(void **)) { */

/* } */

static RegressionResults calculateLinearRegression(int *x, double *y,
                                                   size_t n) {
  double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0, sumY2 = 0;
  for (size_t i = 0; i < n; i++) {
    sumX += x[i];
    sumY += y[i];
    sumXY += (double)x[i] * y[i];
    sumX2 += (double)x[i] * x[i];
    sumY2 += y[i] * y[i];
  }

  double denominator = (n * sumX2 - sumX * sumX);
  RegressionResults res = {0};
  if (denominator == 0)
    return res;

  res.slope = (n * sumXY - sumX * sumY) / denominator;
  res.intercept = (sumY - res.slope * sumX) / n;

  // Calculate R-squared
  double num = (n * sumXY - sumX * sumY);
  double den = (n * sumX2 - sumX * sumX) * (n * sumY2 - sumY * sumY);
  res.r_squared = (den == 0) ? 0 : (num * num) / den;

  return res;
}

/**
 * Meta-test: Runs the performance test at various sizes and asserts scaling
 */
void testScalingBehavior(void **state) {
  void (*testPtr)(void **) = *state;
  int sizes[] = {10000, 20000, 30000, 40000, 50000, 100000, 1000000};
  size_t num_points = sizeof(sizes) / sizeof(sizes[0]);
  double times[num_points];

  for (size_t i = 0; i < num_points; i++) {
    TestParams params = {.current_size = sizes[i]};
    void *p_params = &params;

    TimerContext timer;
    timerStart(&timer);
    testPtr(&p_params);
    timerStop(&timer);

    times[i] = timerGetSeconds(&timer);
    printf("Size %d: %f s\n", sizes[i], times[i]);
  }

  RegressionResults res = calculateLinearRegression(sizes, times, num_points);

  printf("\nScaling Analysis:\nSlope (a): %e s/item\nR^2 (Linearity): %f\n",
         res.slope, res.r_squared);

  // ASSERTIONS:
  // 1. Ensure slope is positive and reasonable (example threshold)
  assert_true(res.slope > 0);
  // 2. Ensure linearity is high (R^2 > 0.99 is generally very linear)
  assert_true(res.r_squared > 0.99);
}

void captureMemoryState(MemoryState *state) {
  for (int i = 0; i < 256; i++) {
    state->counts[i] = allocationCount[i];
  }
}

void assertMemoryBalance(MemoryState *before, MemoryState *after) {
  for (int i = 0; i < 256; i++) {
    assert_int_equal(before->counts[i], after->counts[i]);
  }
}

void assertMemoryBalanceExcept(MemoryState *before, MemoryState *after,
                               int except_types[], size_t num_exceptions) {
  for (int i = 0; i < 256; i++) {
    bool skip = false;
    for (size_t j = 0; j < num_exceptions; j++) {
      if (i == except_types[j] -
                   1) { // object types are 1-indexed in allocationCount array
        skip = true;
        break;
      }
    }
    if (!skip) {
      assert_int_equal(before->counts[i], after->counts[i]);
    }
  }
}

void assertMemoryDifference(MemoryState *before, MemoryState *after, int type,
                            int expected_diff) {
  uword_t start = before->counts[type - 1];
  uword_t end = after->counts[type - 1];
  assert_int_equal(start + expected_diff, end);
}
