#include "../ConcurrentHashMap.h"
#include "../Hash.h"
#include "../PersistentList.h"
#include "TestTools.h"
#include "runtime/Object.h"
#include "runtime/RuntimeInterface.h"
#include <math.h>

#define THREAD_COUNT 10

static bool Ptr_isShared(void *p) {
  return (Object_getRawRefCount((Object *)p) & SHARED_BIT) != 0;
}

typedef struct HashThreadParams {
  int start;
  int stop;
  ConcurrentHashMap *map;
} HashThreadParams;

static void *startThread(void *param) {
  Ebr_register_thread();
  HashThreadParams *p = (HashThreadParams *)param;
  ConcurrentHashMap *l = p->map;

  for (int i = p->start; i < p->stop; i++) {
    RTValue n = RT_boxInt32(i);
    ConcurrentHashMap_assoc(l, n, n);
  }

  Ebr_unregister_thread();
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

static void test_concurrent_hash_map_overcrowded(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // Create a very small map to ensure it gets overcrowded quickly
    // initialSizeExponent = 2 -> size 4
    ConcurrentHashMap *m = ConcurrentHashMap_create(2);

    // Filling it should eventually trigger the overcrowded exception
    // resizingThreshold = MIN(round(pow(4, 0.33)), 128) = MIN(1.58, 128) = 1
    // (approx) It's very small, so a few assocs should trigger it.

    ASSERT_THROWS("IllegalStateException", {
      for (int i = 0; i < 100; i++) {
        ConcurrentHashMap_assoc(m, RT_boxInt32(i), RT_boxInt32(i));
      }
    });

    Ptr_release(m);
  });
}

static void test_chm_promotion_on_assoc(void **state) {
  (void)state;
  // Initialize PersistentList_empty() singleton before the block
  // because otherwise it will be recorded as a "leak" (type 8)
  PersistentList_empty();
  ASSERT_MEMORY_ALL_BALANCED({
    // 1. Test shared-at-birth
    ConcurrentHashMap *m = ConcurrentHashMap_create(3);
    assert_true(Ptr_isShared(m));

    // 2. Test promotion on assoc
    PersistentList *l =
        PersistentList_create(RT_boxInt32(1), PersistentList_empty());
    assert_false(Ptr_isShared(l));

    ConcurrentHashMap_assoc(m, RT_boxPtr(l), RT_boxInt32(100));
    // The list should have been promoted because it was inserted into CHM
    assert_true(Ptr_isShared(l));

    Ptr_release(m);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_chm_promotion_on_assoc),
      cmocka_unit_test_prestate(testScalingBehavior,
                                concurrentMapThreadingAndPerformance),
      cmocka_unit_test(test_concurrent_hash_map_overcrowded),
  };
  initialise_memory();
  srand(0);
  int x = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return x;
}
