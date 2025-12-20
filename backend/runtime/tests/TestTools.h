#ifndef RUNTIME_TESTS
#define RUNTIME_TESTS

#include <time.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <gmp.h>
#include "../defines.h"
#include "../Object.h"
#include <time.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>

#include "cmocka.h"


//#include <gperftools/profiler.h>

void pd();

// A structure to hold the start and end clock times
typedef struct {
    clock_t start;
    clock_t end;
} TimerContext;

typedef struct {
    double slope;      // 'a' in y = ax + b
    double intercept;  // 'b' in y = ax + b
    double r_squared;  // Linearity: 1.0 is perfectly linear
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

#endif
