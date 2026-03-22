#ifndef RUNTIME_TESTS
#define RUNTIME_TESTS

#include "../Object.h"
#include "../RuntimeInterface.h"
#include "../defines.h"
#include <assert.h>
#include <gmp.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdarg.h>
#ifdef __cplusplus
#include <atomic>
using std::atomic_compare_exchange_strong_explicit;
using std::atomic_compare_exchange_weak_explicit;
using std::atomic_exchange_explicit;
using std::atomic_fetch_add_explicit;
using std::atomic_fetch_sub_explicit;
using std::atomic_load_explicit;
using std::atomic_store_explicit;
using std::memory_order;
using std::memory_order_acq_rel;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_seq_cst;
#define _Atomic(X) std::atomic<X>
#else
#include <stdatomic.h>
#endif
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
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wstringop-overflow\"") if (         \
            strstr(BUILD_TYPE, "Release")) {                                   \
      fail_msg("ASSERT_MEMORY_BALANCE cannot be used in Release mode as "      \
               "memory tracking results may be unreliable or disabled.");      \
    }                                                                          \
    _Pragma("GCC diagnostic pop") uword_t start_count =                        \
        allocationCount[(type)-1];                                             \
    block uword_t end_count = allocationCount[(type)-1];                       \
    assert_int_equal(start_count, end_count);                                  \
  } while (0)

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
    if (!strstr(BUILD_TYPE, "Debug")) {                                        \
      fail_msg("ASSERT_MEMORY_ALL_BALANCED must be used in Debug mode as "     \
               "memory tracking results are disabled in Release mode.");       \
    }                                                                          \
    MemoryState __before, __after;                                             \
    captureMemoryState(&__before);                                             \
    {block} captureMemoryState(&__after);                                      \
    assertMemoryBalance(&__before, &__after);                                  \
  } while (0)

extern jmp_buf exception_env;
extern char *last_exception_name;
extern bool exception_catchable;

void reset_exception_state();

#define ASSERT_THROWS(expected_name, block)                                    \
  do {                                                                         \
    reset_exception_state();                                                   \
    exception_catchable = true;                                                \
    if (setjmp(exception_env) == 0) {                                          \
      {block} exception_catchable = false;                                     \
      fail_msg("Expected exception '%s' was not thrown", expected_name);       \
    } else {                                                                   \
      exception_catchable = false;                                             \
      assert_string_equal(last_exception_name, expected_name);                 \
    }                                                                          \
  } while (0)

#ifdef __cplusplus
#define assert_string_contains(str, substr)                                    \
  do {                                                                         \
    std::string __s = (str);                                                   \
    const char *__sub = (substr);                                              \
    if (__s.find(__sub) == std::string::npos) {                                \
      fail_msg("String '%s' does not contain '%s'", __s.c_str(), __sub);       \
    }                                                                          \
  } while (0)

#define assert_string_not_contains(str, substr)                                \
  do {                                                                         \
    std::string __s = (str);                                                   \
    const char *__sub = (substr);                                              \
    if (__s.find(__sub) != std::string::npos) {                                \
      fail_msg("String '%s' contains '%s' (but should not)", __s.c_str(),      \
               __sub);                                                         \
    }                                                                          \
  } while (0)
#else
#define assert_string_contains(str, substr)                                    \
  do {                                                                         \
    const char *__s = (str);                                                   \
    const char *__sub = (substr);                                              \
    if (strstr(__s, __sub) == NULL) {                                          \
      fail_msg("String '%s' does not contain '%s'", __s, __sub);               \
    }                                                                          \
  } while (0)

#define assert_string_not_contains(str, substr)                                \
  do {                                                                         \
    const char *__s = (str);                                                   \
    const char *__sub = (substr);                                              \
    if (strstr(__s, __sub) != NULL) {                                          \
      fail_msg("String '%s' contains '%s' (but should not)", __s, __sub);      \
    }                                                                          \
  } while (0)
#endif

#endif
