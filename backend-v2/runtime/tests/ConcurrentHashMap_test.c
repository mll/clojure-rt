#include "TestTools.h"
#include <math.h>

#define THREAD_COUNT 10

typedef struct HashThreadParams {
  int start;
  int stop;
  ConcurrentHashMap *map;
} HashThreadParams;

static void *startThread(void *param) {
  HashThreadParams *p = (HashThreadParams *)param;
  ConcurrentHashMap *l = p->map;

  for (int i = p->start; i < p->stop; i++) {
    RTValue n = RT_boxInt32(i);
    ConcurrentHashMap_assoc(l, n, n);
  }

  return NULL;
}

static void concurrentMapThreadingAndPerformance(void **state) {
  TestParams *testParams = (TestParams *)*state;
  size_t size = testParams->current_size;
  TimerContext timer;
  timerStart(&timer);
  pd();

  ConcurrentHashMap *l = ConcurrentHashMap_create(((int)log2(size)) + 3);
  word_t perThread = size / THREAD_COUNT;
  timerStop(&timer);
  printf("\nConcurrentHashMap start. Time: %f seconds\n",
         timerGetSeconds(&timer));
  timerStart(&timer);
  HashThreadParams params[THREAD_COUNT];
  pthread_t threads[THREAD_COUNT];

  for (word_t i = 0; i < THREAD_COUNT; i++) {
    params[i].start = i * perThread;
    params[i].stop = (i + 1) * perThread;
    params[i].map = l;
    pthread_create(&(threads[i]), NULL, startThread, (void *)&params[i]);
  }

  for (word_t i = 0; i < THREAD_COUNT; i++)
    pthread_join(threads[i], NULL);

  timerStop(&timer);
  double assoc_time = timerGetSeconds(&timer);

  printf("\nConcurrentHashMap assoc. Time: %f seconds\n", assoc_time);
  pd();

  timerStart(&timer);

  word_t sum = 0;
  RTValue k = RT_boxInt32(1);
  for (word_t i = 0; i < (word_t)size; i++) {
    k = RT_boxInt32(i);
    RTValue o = ConcurrentHashMap_get(l, k);
    assert_true(o);
    if (getType(o) != integerType) {
      printf("Unknown type %d for entry %s\n", getType(o),
             String_c_str(toString(k)));
      o = ConcurrentHashMap_get(l, k);
      printf("Unknown type %d for entry %s\n", getType(o),
             String_c_str(toString(k)));
      Ptr_retain(l);
      retain(o);
      printf("Contents: %s %s\n", String_c_str(toString(o)),
             String_c_str(String_compactify(toString(RT_boxPtr(l)))));
    }

    assert_true(getType(o) == integerType);
    int32_t res = RT_unboxInt32(o);
    assert(res == i);
    sum += res;
  }

  timerStop(&timer);
  double get_time = timerGetSeconds(&timer);

  printf("\nConcurrentHashMap get. Time: %f seconds\n", get_time);
  pd();
  assert_int_equal(sum, (size - 1) * size / 2);

  timerStart(&timer);
  Ptr_release(l);
  timerStop(&timer);
  printf("\nConcurrentHashMap release. Time: %f seconds\n",
         timerGetSeconds(&timer));
  pd();
}

int main(void) {
  const struct CMUnitTest tests[] = {
      // There are problems with the map - bad performance all around and
      // possible
      // race conditions.
      // Needs to be re-implemented? or at least checked with emini
      cmocka_unit_test_prestate(testScalingBehavior,
                                concurrentMapThreadingAndPerformance),
  };
  initialise_memory();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
