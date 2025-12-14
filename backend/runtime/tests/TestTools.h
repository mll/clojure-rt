#ifndef RUNTIME_TESTS
#define RUNTIME_TESTS

//#include "Object.h"
//#include "PersistentList.h"
//#include "PersistentVector.h"
//#include "PersistentVectorNode.h"
//#include "Integer.h"
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

//#include <gperftools/profiler.h>

void pd();

// A structure to hold the start and end clock times
typedef struct {
    clock_t start;
    clock_t end;
} TimerContext;

// Function declarations
void timer_start(TimerContext *context);
void timer_stop(TimerContext *context);
double timer_get_seconds(const TimerContext *context);

#endif
