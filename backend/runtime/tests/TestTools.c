#include "TestTools.h"

//#include <gperftools/profiler.h>

void pd() {
    printf("Ref counters: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n", allocationCount[0], allocationCount[1], allocationCount[2], allocationCount[3], allocationCount[4], allocationCount[5], allocationCount[6], allocationCount[7], allocationCount[8], allocationCount[9], allocationCount[10], allocationCount[11], allocationCount[12]);
}


void timer_start(TimerContext *context) {
    if (context) {
        context->start = clock();
    }
}

void timer_stop(TimerContext *context) {
    if (context) {
        context->end = clock();
    }
}

double timer_get_seconds(const TimerContext *context) {
    if (!context) {
        return 0.0;
    }
    // Calculate the difference and convert clock ticks to seconds
    return (double)(context->end - context->start) / CLOCKS_PER_SEC;
}


void testVector (bool pauses) {
  printf("=========================== STARTING VECTOR TESTS =============================================\n");
  // printf("Total size: %lu %lu\n", sizeof(Object), sizeof(Integer)); 
  // printf("Total size: %lu %lu\n", sizeof(PersistentVector), sizeof(PersistentVectorNode)); 
  PersistentVector *l = PersistentVector_create();
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  if(pauses) printf("Press a key to start\n");
  pd();
  if(pauses) getchar();
  printf("Creating vector using standard methods (reuse: false) \n");
  clock_t as = clock();

  bool reuse = false;
  size_t size = 100000000;
  
  for (size_t i = 0; i<size; i++) {
   // PersistentVector_print(l);
   // printf("=======*****************===========");
   // fflush(stdout);
    Integer *n = Integer_create(i);
    if(!reuse) retain(l);
    PersistentVector *k = PersistentVector_conj(l, n);
    if(!reuse) release(l);

    l = k;
    // printf("%d\r", i);
    // fflush(stdout);
//    PersistentVector_print(l);
  }
  pd();
  clock_t ap = clock();
  #ifdef REFCOUNT_NONATOMIC
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, ((Object *)l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
  #else 
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, atomic_load(&(((Object *)l)->atomicRefCount)), (double)(ap - as) / CLOCKS_PER_SEC);
  #endif
  if (pauses) getchar();
  printf("releasing original vector: \n");
  release(l);


  if(pauses) getchar();
  printf("Creating vector using standard methods (reuse: true) \n");
  as = clock();

  reuse = true;
  l = PersistentVector_create();
  for (size_t i=0;i<size; i++) {
   // PersistentVector_print(l);
   // printf("=======*****************===========");
   // fflush(stdout);
    Integer *n = Integer_create(i);
    if(!reuse) retain(l);
    PersistentVector *k = PersistentVector_conj(l, n);
    if(!reuse) release(l);

    l = k;
    // printf("%d\r", i);
    // fflush(stdout);
//    PersistentVector_print(l);
  }
  pd();
  ap = clock();
  #ifdef REFCOUNT_NONATOMIC
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, ((Object *)l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
  #else 
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, atomic_load(&(((Object *)l)->atomicRefCount)), (double)(ap - as) / CLOCKS_PER_SEC);
  #endif
  if (pauses) getchar();
  printf("releasing original vector: \n");
  release(l);
  


  pd();
  printf("Creating vector using transients\n");
  clock_t as9 = clock();
  l = PersistentVector_transient(PersistentVector_create());
  
  for (size_t i=0;i<size; i++) {
   // PersistentVector_print(l);
   // printf("=======*****************===========");
   // fflush(stdout);
    Integer *n = Integer_create(i);
    retain(l);
    PersistentVector *k = PersistentVector_conj_BANG_(l, n);
    release(l);
    l = k;
  }
  l = PersistentVector_persistent_BANG_(l);
  pd();
  clock_t ap9 = clock();
  #ifdef REFCOUNT_NONATOMIC
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, ((Object *)l)->refCount, (double)(ap9 - as9) / CLOCKS_PER_SEC);
  #else 
  printf("Vector size: %llu\nRef count: %llu\nTime: %f\n", l->count, atomic_load(&(((Object *)l)->atomicRefCount)), (double)(ap9 - as9) / CLOCKS_PER_SEC);
  #endif



  printf("Summing using nth");

  clock_t os = clock();
  
  int64_t sum = 0;
  
  for(size_t i=0; i< l->count; i++) {
    retain(l);
    Integer *ob = (Integer *) PersistentVector_nth(l, i);
    sum += (ob)->value;
    release(ob);
  }

  clock_t op = clock();
  printf("Sum: %llu\nTime: %f\n", sum, (double)(op - os) / CLOCKS_PER_SEC);
//  assert(sum == 4999999950000000ull && "Wrong result");
  if(pauses) getchar();

  printf("Summing using iterator");

  clock_t os9 = clock();
  
  int64_t sum9 = 0;
  
  PersistentVectorIterator it = PersistentVector_iterator(l);
  
  Object *obss = PersistentVector_iteratorGet(&it);
  while(obss) {
    sum9 += ((Integer *)obss)->value;    
    obss = PersistentVector_iteratorNext(&it);
  }

  clock_t op9 = clock();
  printf("iterator Sum: %llu\nTime: %f\n", sum, (double)(op9 - os9) / CLOCKS_PER_SEC);
  assert(sum9 == 4999999950000000ull && "Wrong result");
  if(pauses) getchar();

  int64_t sum2 = 0;
  int64_t *array = (int64_t *)malloc(size*sizeof(int64_t));
  memset(array, 0, size * sizeof(int64_t));
  for(size_t i=0; i< size; i++) {
    array[i] = i;
  }

  clock_t oss = clock();
  for(size_t i=0; i< l->count; i++) {
    sum2 += array[i];
  }
  clock_t opp = clock();
  free(array);
  printf("Simple array sum: %llu\nTime: %f\n", sum2, (double)(opp - oss) / CLOCKS_PER_SEC);

  if(pauses) getchar();
  printf("Summing array with objects: \n");
  int64_t sum22 = 0;
  Object **array9 = (Object **)malloc(size*sizeof(Object *));
  for(size_t i=0; i< size; i++) {
    array9[i] = (Object *)Integer_create(i);
  }

  clock_t oss9 = clock();
  for(size_t i=0; i < l->count; i++) {
    sum22 += ((Integer *)array9[i])->value;
  }
  clock_t opp9 = clock();
  for(size_t i=0; i< size; i++) {
    Object_release(array9[i]);
  }
  free(array9);
  printf("Simple array with objects sum: %llu\nTime: %f\n", sum22, (double)(opp9 - oss9) / CLOCKS_PER_SEC);

  if(pauses) getchar();
  printf("Summing array with obj - integers: \n");
  sum22 = 0;
  Integer **array99 = (Integer **)malloc(size*sizeof(Integer *));
  for(size_t i=0; i< size; i++) {
    array99[i] = Integer_create(i);
  }

  clock_t oss99 = clock();
  for(size_t i=0; i < l->count; i++) {
    sum22 += array99[i]->value;
  }
  clock_t opp99 = clock();
  for(size_t i=0; i< size; i++) {
    release(array99[i]);
  }
  free(array99);
  printf("Simple array with integer objects sum: %llu\nTime: %f\n", sum22, (double)(opp99 - oss99) / CLOCKS_PER_SEC);



  clock_t ass = clock();
  for (size_t i=0;i<size; i++) {
    // PersistentVector_print(l);
    // printf("=======*****************===========");
    // fflush(stdout);
    reuse = false;
    Integer *n = Integer_create(7);
    if(!reuse) retain(l);
    PersistentVector *k = PersistentVector_assoc(l, i, n);
    if(!reuse) release(l);
    l = k;
//    printf("%d\r",i);
  }

  sum = 0;
  
  for(size_t i=0; i< l->count; i++) {
    retain(l);
    Integer *ob = (Integer *) PersistentVector_nth(l, i);
    sum += ob->value;
    release(ob);
  }

  clock_t asd = clock();
  printf("Assocs + sum: %llu\nTime: %f\n", sum, (double)(asd - ass) / CLOCKS_PER_SEC);
  clock_t ds = clock();
  #ifdef REFCOUNT_NONATOMIC
  printf("%llu\n", ((Object *)l)->refCount);
  #else
  printf("%llu\n", atomic_load(&(((Object *)l)->atomicRefCount)));
  #endif
  release(l);
  pd();
  clock_t dp = clock();
  printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
}


