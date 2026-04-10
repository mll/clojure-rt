#include "../ConcurrentHashMap.h"
#include "../Ebr.h"
#include "../Hash.h"
#include "../Object.h"
#include "../PersistentList.h"
#include "../RTValue.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "TestTools.h"
#include <math.h>
#include <unistd.h>

#define THREAD_COUNT 10
#define BATTLE_THREADS 24
#define BATTLE_KEYS 500
#define BATTLE_DURATION_MS 2000

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
  Ebr_enter_critical();
  HashThreadParams *p = (HashThreadParams *)param;
  ConcurrentHashMap *l = p->map;

  for (int i = p->start; i < p->stop; i++) {
    RTValue n = RT_boxInt32(i);
    Ptr_retain(l);
    ConcurrentHashMap_assoc(l, n, n);
    Ebr_flush_critical();
  }

  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

static void concurrentMapThreadingAndPerformance(void **state) {
  TestParams *testParams = (TestParams *)*state;
  size_t size = testParams->current_size;
  TimerContext timer;
  timerStart(&timer);
  pd();

  ConcurrentHashMap *l = ConcurrentHashMap_create(((int)log2(size)) + 5);
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
    Ptr_retain(l);
    RTValue o = ConcurrentHashMap_get(l, k);
    assert_true(o);
    if (getType(o) != integerType) {
      printf("Unknown type %d for entry %s\n", getType(o),
             String_c_str(toString(k)));
    }

    assert_true(getType(o) == integerType);
    int32_t res = RT_unboxInt32(o);
    assert(res == i);
    sum += res;
    Ebr_flush_critical();
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
  Ebr_force_reclaim();
}

static void test_concurrent_hash_map_overcrowded(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ConcurrentHashMap *m = ConcurrentHashMap_create(2);
    ASSERT_THROWS("IllegalStateException", {
      for (int i = 0; i < 100; i++) {
        Ptr_retain(m);
        ConcurrentHashMap_assoc(m, RT_boxInt32(i), RT_boxInt32(i));
      }
    });
    Ptr_release(m);
  });
}

static void test_chm_promotion_on_assoc(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ConcurrentHashMap *m = ConcurrentHashMap_create(2);
    RTValue k = RT_boxPtr(
        PersistentList_create(RT_boxInt32(1), PersistentList_empty()));
    RTValue v = RT_boxPtr(PersistentVector_createMany(0));

    assert_false(Ptr_isShared((void *)RT_unboxPtr(k)));
    assert_false(Ptr_isShared((void *)RT_unboxPtr(v)));
    assert_true(Ptr_isShared(m));

    Ptr_retain(m);
    retain(k);
    retain(v);
    ConcurrentHashMap_assoc(m, k, v);
    assert_true(Ptr_isShared((void *)RT_unboxPtr(k)));
    assert_true(Ptr_isShared((void *)RT_unboxPtr(v)));
    assert_true(Ptr_isShared(m));
    release(k);
    release(v);
    // 2. Test promotion on assoc
    PersistentList *l =
        PersistentList_create(RT_boxInt32(1), PersistentList_empty());
    Ptr_retain(l);
    assert_false(Ptr_isShared(l));

    Ptr_retain(m);
    ConcurrentHashMap_assoc(m, RT_boxPtr(l), RT_boxInt32(100));
    // The list should have been promoted because it was inserted into CHM
    assert_true(Ptr_isShared(l));

    Ptr_release(l);
    Ptr_release(m);
    // Triggering two epochs to reclaim the value;
    Ebr_force_reclaim();
  });
}

typedef struct RWThreadParams {
  ConcurrentHashMap *map;
  atomic_bool running;
  int workerId;
} RWThreadParams;

static void *writerThread(void *param) {
  Ebr_register_thread();
  Ebr_enter_critical();
  RWThreadParams *p = (RWThreadParams *)param;
  unsigned int seed = (unsigned int)time(NULL) + p->workerId;
  while (atomic_load(&p->running)) {
    int k_val = rand_r(&seed) % BATTLE_KEYS;
    RTValue k = RT_boxInt32(k_val);
    RTValue v;

    if (rand_r(&seed) % 5 == 0) { // 20% dissoc
      Ptr_retain(p->map);
      ConcurrentHashMap_dissoc(p->map, k);
    } else {
      // 90% of values are strings to stress EBR
      if (rand_r(&seed) % 10 != 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "val-%d", (k_val ^ 0xDEADC0DE));
        v = RT_boxPtr(String_createDynamicStr(buf));
      } else {
        v = RT_boxInt32(k_val ^ 0xDEADC0DE);
      }
      Ptr_retain(p->map);
      ConcurrentHashMap_assoc(p->map, k, v);
    }
    Ebr_flush_critical();
  }
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

static void *readerThread(void *param) {
  Ebr_register_thread();
  Ebr_enter_critical();
  RWThreadParams *p = (RWThreadParams *)param;
  unsigned int seed = (unsigned int)time(NULL) + p->workerId;
  while (atomic_load(&p->running)) {
    int k_val = rand_r(&seed) % BATTLE_KEYS;
    RTValue k = RT_boxInt32(k_val);
    Ptr_retain(p->map);
    RTValue v = ConcurrentHashMap_get(p->map, k);
    if (!RT_isNil(v)) {
      if (RT_isPtr(v) && getType(v) == stringType) {
        String *s = (String *)RT_unboxPtr(v);
        char buf[32];
        snprintf(buf, sizeof(buf), "val-%d", (k_val ^ 0xDEADC0DE));
        assert_string_equal(String_c_str(s), buf);
        release(v);
      } else {
        assert_true(RT_isInt32(v));
        assert_int_equal((uint32_t)RT_unboxInt32(v),
                         (uint32_t)(k_val ^ 0xDEADC0DE));
      }
    }
    Ebr_flush_critical();
  }
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

static void test_concurrent_read_write(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ConcurrentHashMap *m = ConcurrentHashMap_create(10);
    RWThreadParams params[BATTLE_THREADS];
    pthread_t threads[BATTLE_THREADS];

    for (int i = 0; i < BATTLE_THREADS; i++) {
      params[i].map = m;
      params[i].workerId = i;
      atomic_init(&params[i].running, true);
      if (i < BATTLE_THREADS / 2)
        pthread_create(&threads[i], NULL, writerThread, &params[i]);
      else
        pthread_create(&threads[i], NULL, readerThread, &params[i]);
    }

    usleep(BATTLE_DURATION_MS * 1000);
    for (int i = 0; i < BATTLE_THREADS; i++)
      atomic_store(&params[i].running, false);

    for (int i = 0; i < BATTLE_THREADS; i++)
      pthread_join(threads[i], NULL);

    Ptr_release(m);
    // Cleanup EBR
    Ebr_force_reclaim();
  });
}

static void test_chm_with_strings(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ConcurrentHashMap *m = ConcurrentHashMap_create(10);
    for (int i = 0; i < 100; i++) {
      char buf[32];
      snprintf(buf, sizeof(buf), "key-%d", i);
      RTValue k = RT_boxPtr(String_createDynamicStr(buf));
      snprintf(buf, sizeof(buf), "val-%d", i);
      RTValue v = RT_boxPtr(String_createDynamicStr(buf));

      Ptr_retain(m);
      ConcurrentHashMap_assoc(m, k, v);
    }

    for (int i = 0; i < 100; i++) {
      char buf[32];
      snprintf(buf, sizeof(buf), "key-%d", i);
      RTValue k = RT_boxPtr(String_createDynamicStr(buf));
      Ptr_retain(m);
      RTValue v = ConcurrentHashMap_get(m, k);
      assert_true(RT_isPtr(v));
      assert_true(getType(v) == stringType);
      release(v);
    }
    Ptr_release(m);
    // Trigger reclamation of EBR-retired values (need 2 passes)
    Ebr_force_reclaim();
  });
}

// static void test_ebr_reclamation_threshold(void **state) {
//   (void)state;
//   ASSERT_MEMORY_ALL_BALANCED({
//     ConcurrentHashMap *m = ConcurrentHashMap_create(14);
//     size_t reclaimation_threshold = 32;

//     // Initial fill
//     for (size_t i = 0; i < reclaimation_threshold; i++) {
//       Ptr_retain(m);
//       ConcurrentHashMap_assoc(m, RT_boxInt32(i),
//                               RT_boxPtr(String_createDynamicStr("initial")));
//     }

//     // Now replace all values to trigger 1024 "autorelease" calls
//     for (size_t i = 0; i < reclaimation_threshold; i++) {
//       Ptr_retain(m);
//       ConcurrentHashMap_assoc(m, RT_boxInt32(i),
//                               RT_boxPtr(String_createDynamicStr("updated")));
//     }

//     // size_t pending = Ebr_get_pending_count();
//     // printf("Pending after %zu replacements: %zu\n",
//     reclaimation_threshold,
//     //        pending);
//     // assert_true(pending >= reclaimation_threshold);

//     int retries = 0;
//     while (Ebr_get_pending_count() > 0 && retries < 200) {
//       usleep(10000);
//       retries++;
//     }

//     Ptr_release(m);

//     retries = 0;
//     while (Ebr_get_pending_count() > 0 && retries < 200) {
//       usleep(10000);
//       retries++;
//     }

//     printf("Final pending count: %zu\n", Ebr_get_pending_count());
//     assert_true(Ebr_get_pending_count() == 0);
//   });
// }

static void test_chm_to_string(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ConcurrentHashMap *m = ConcurrentHashMap_create(2);
    // Add some strings
    Ptr_retain(m);
    ConcurrentHashMap_assoc(m, RT_boxPtr(String_createDynamicStr("key1")),
                            RT_boxPtr(String_createDynamicStr("value1")));
    Ptr_retain(m);
    ConcurrentHashMap_assoc(m, RT_boxPtr(String_createDynamicStr("key2")),
                            RT_boxPtr(String_createDynamicStr("value2")));

    String *compact = String_compactify(toString(RT_boxPtr(m)));
    printf("Map string: %s\n", String_c_str(compact));

    Ptr_retain(compact);
    assert_true(String_contains(compact, String_createStatic("key1")));
    Ptr_retain(compact);
    assert_true(String_contains(compact, String_createStatic("value1")));
    Ptr_retain(compact);
    assert_true(String_contains(compact, String_createStatic("key2")));
    assert_true(String_contains(compact, String_createStatic("value2")));
    // Values are EBR managed, EBR flush is due.
    Ebr_force_reclaim();
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_chm_promotion_on_assoc),
      cmocka_unit_test_prestate(testScalingBehavior,
                                concurrentMapThreadingAndPerformance),
      cmocka_unit_test(test_concurrent_hash_map_overcrowded),
      cmocka_unit_test(test_concurrent_read_write),
      cmocka_unit_test(test_chm_with_strings),
      // cmocka_unit_test(test_ebr_reclamation_threshold),
      cmocka_unit_test(test_chm_to_string),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  Ebr_enter_critical();
  srand(0);
  int x = cmocka_run_group_tests(tests, NULL, NULL);
  Ebr_leave_critical();
  RuntimeInterface_cleanup();
  return x;
}
