#include "TestTools.h"

#define THREAD_COUNT 10

typedef struct HashThreadParams {
  int start;
  int stop;
  ConcurrentHashMap *map;
} HashThreadParams;

static void *startThread(void *param) {
  HashThreadParams *p = (HashThreadParams *)param;
  ConcurrentHashMap *l = p->map;
  
  for(int i=p->start; i<p->stop; i++) {
    Integer *n = Integer_create(i);
    retain(n);
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
  
  ConcurrentHashMap *l = ConcurrentHashMap_create(28);
  int perThread = size / THREAD_COUNT;

  HashThreadParams params[THREAD_COUNT];
  pthread_t threads[THREAD_COUNT];

  for(int i=0; i<THREAD_COUNT; i++) {
    params[i].start = i * perThread;
    params[i].stop = (i+1) * perThread;
    params[i].map = l;
    pthread_create(&(threads[i]), NULL, startThread, (void *) &params[i]);
  }
  
  for(int i=0; i<THREAD_COUNT; i++) pthread_join(threads[i], NULL);


  timerStop(&timer);
  double assoc_time = timerGetSeconds(&timer);

  printf("\nConcurrentHashMap assoc. Time: %f seconds\n", assoc_time);
  pd();

  timerStart(&timer);
  
  int64_t sum = 0;
  Integer *k = Integer_create(1);
  for(int64_t i=0; i< (int) size; i++) {
    k->value = i;
    retain(k);
    Object *o = (Object *)ConcurrentHashMap_get(l, k);
    assert_true(o);
    if(o->type != integerType) {
      retain(k);
      printf("Unknown type %d for entry %s\n", o->type, String_c_str(toString(k)));
      retain(k);
      o = (Object *)ConcurrentHashMap_get(l, k);
      retain(k);
      printf("Unknown type %d for entry %s\n", o->type, String_c_str(toString(k)));
      retain(l);
      retain(o);
      printf("Contents: %s %s\n", String_c_str(toString(o)),
             String_c_str(String_compactify(toString(l))));
    }

    assert_true(o->type == integerType);
    Integer *res = (Integer *) o;
    assert(res->value == i);
    sum += res->value;
    release(res);
  }
  
  release(k);

  timerStop(&timer);
  double get_time = timerGetSeconds(&timer);

  printf("\nConcurrentHashMap get. Time: %f seconds\n", get_time);
  pd();
  assert_int_equal(sum, (size - 1) * size / 2);  


  timerStart(&timer);
  release(l);
  timerStop(&timer);  
  printf("\nConcurrentHashMap release. Time: %f seconds\n", timerGetSeconds(&timer));
  pd();
}

int main(void) {
  const struct CMUnitTest tests[] = {
    // There are problems with the map - bad performance all around and possible
    // race conditions.
    // Needs to be re-implemented? or at least checked with emini 
        cmocka_unit_test_prestate(testScalingBehavior, concurrentMapThreadingAndPerformance),
    };
    initialise_memory();
    srand(0);
    return cmocka_run_group_tests(tests, NULL, NULL);
}


