
//#include "Object.h"
//#include "PersistentList.h"
//#include "PersistentVector.h"
//#include "PersistentVectorNode.h"
//#include "Integer.h"
#include <time.h>
#include <stdio.h>
#include <cstdint>
#include <stdatomic.h>
#include <cstring>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <gmp.h>

extern "C" {
#ifndef restrict
#  if defined(__cplusplus)
#    define restrict __restrict
#  endif
#endif
   #include "defines.h"
   #include "Object.h"
}

//#include <gperftools/profiler.h>


void pd() {
    printf("Ref counters: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n", allocationCount[0], allocationCount[1], allocationCount[2], allocationCount[3], allocationCount[4], allocationCount[5], allocationCount[6], allocationCount[7], allocationCount[8], allocationCount[9], allocationCount[10], allocationCount[11], allocationCount[12]);
}


typedef struct HashThreadParams {
  int start;
  int stop;
  ConcurrentHashMap *map;
} HashThreadParams;

void *startThread(void *param) {
  HashThreadParams *p = (HashThreadParams *)param;
  ConcurrentHashMap *l = p->map;
  
  for(int i=p->start; i<p->stop; i++) {
    Integer *n = Integer_create(i);
    retain(n);
    ConcurrentHashMap_assoc(l, n, n);
  }
 
  return NULL;
}


void testConcurrentMap (bool pauses) {
    printf("=========================== STARTING CONCURRENT MAP TESTS =============================================\n");
  ConcurrentHashMap *l = ConcurrentHashMap_create(28);
  // // l = l->conj(new Number(3));
  // // l = l->conj(new Number(7));
  // // l = l->conj(new PersistentList(new Number(2)));
  // // l = l->conj(new PersistentList());
  printf("Press a key to start\n");
  pd();
  if(pauses) getchar();
  struct timeval as, ap;
  gettimeofday(&as, NULL);
  int threadNo = 1;
  int perThread = 100000000 / threadNo;

  HashThreadParams params[10];
  pthread_t threads[10];

  for(int i=0; i<threadNo; i++) {
    params[i].start = i * perThread;
    params[i].stop = (i+1) * perThread;
    params[i].map = l;
    pthread_create(&(threads[i]), NULL, startThread, (void *) &params[i]);
  }
  
  for(int i=0; i<10; i++) pthread_join(threads[i], NULL);

  gettimeofday(&ap, NULL);

  printf("Time: %f\n", (ap.tv_sec - as.tv_sec) + (ap.tv_usec - as.tv_usec)/1000000.0);

  pd();

  struct timeval ss, sp;
  gettimeofday(&ss, NULL);
  int64_t sum = 0;
  Integer *k = Integer_create(1);
  for(int i=0; i< 100000000; i++) {
    k->value = i;
    retain(k);
    Object *o = (Object *)ConcurrentHashMap_get(l, k);
    assert(o);
    if(o->type != integerType) {
      retain(k);
      printf("Unknown type %d for entry %s\n", o->type, String_c_str(toString(k)));
      retain(k);
      o = (Object *)ConcurrentHashMap_get(l, k);
      retain(k);
      printf("Unknown type %d for entry %s\n", o->type, String_c_str(toString(k)));
      retain(l);
      retain(o);
      printf("Contents: %s %s\n", String_c_str(toString(o)), String_c_str(String_compactify(toString(l))));
    }

    assert(o->type == integerType);
    Integer *res = (Integer *) o;
    assert(res->value == i);
    sum += res->value;
    release(res);
  }
  
  
  release(k);
  gettimeofday(&sp, NULL);
//  retain(l);
//  printf("Contents: %s \n", String_c_str(String_compactify(toString(l))));
  printf("Sum: %llu, Time: %f\n", sum, (sp.tv_sec - ss.tv_sec) + (sp.tv_usec - ss.tv_usec)/1000000.0);
  assert(sum == 4999999950000000ULL && "Wrong result");

  clock_t ds = clock();
  release(l);
  clock_t dd = clock();
  printf("Release Time: %f\n", (double)(dd - ds) / CLOCKS_PER_SEC);
  pd();
  // for (int i=0;i<100000000; i++) {
  //   Integer *n = Integer_create(i);
  //   PersistentList *k = PersistentList_conj(l, super(n));
  //   l = k;
  // }
  // pd();
  // clock_t ap = clock();
  // printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, ((Object *)l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
   
  // if (pauses) getchar();
  // clock_t os = clock();

  // PersistentList *tmp = l;
  // int64_t sum = 0;
  // while(tmp != NULL) {
  //   if(tmp->first) sum += ((Integer *)(tmp->first))->value;
  //   tmp = tmp->rest;
  // }
  // clock_t op = clock();
  // printf("Sum: %llu\nTime: %f\n", sum, (double)(op - os) / CLOCKS_PER_SEC);
  // if(pauses) getchar();
  // clock_t ds = clock();
  // printf("%llu\n", ((Object *)l)->refCount);
  // release(l);
  // pd();
  // clock_t dp = clock();
  // printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
}



void testList (bool pauses) {
  printf("=========================== STARTING LIST TESTS =============================================\n");
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  size_t size = 100000000;
  if(pauses) printf("Press a key to start\n");
  printf("Memory counters before test:\n");
  pd();
  printf("Allocating a list and conjoining a lot of integers:\n");
  if(pauses) getchar();
  clock_t as = clock();

  PersistentList *l = PersistentList_create(NULL, NULL);
  for (size_t i=0; i < size; i++) {
    Integer *n = Integer_create(i);
    PersistentList *k = PersistentList_conj(l, n);
    l = k;
  }
  // retain(l);
  // String *s = String_compactify(toString(l));
  // char *text = String_c_str(s);
  // printf("LIST: %s\n", text);
  clock_t ap = clock();
  printf("Memory counters after initial allocation:\n");
  pd();
  #ifdef REFCOUNT_NONATOMIC
  printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, ((Object *)l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
  #else 
  printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, atomic_load(&(((Object *) l)->atomicRefCount)), (double)(ap - as) / CLOCKS_PER_SEC);
  #endif

  if (pauses) getchar();
  clock_t os = clock();

  printf("Computing sum:\n");
  PersistentList *tmp = l;
  int64_t sum = 0;
  while(tmp != NULL) {
    if(tmp->first) sum += ((Integer *)(tmp->first))->value;
    tmp = tmp->rest;
  }
  assert(sum == 4999999950000000ull && "Wrong result");
  clock_t op = clock();
  printf("Sum: %llu\nTime: %f\n", sum, (double)(op - os) / CLOCKS_PER_SEC);
  if(pauses) getchar();
#ifdef REFCOUNT_NONATOMIC
  printf("Refcount for list: %llu\n", ((Object *)l)->refCount);
#else 
  printf("Refcount for list: %llu\n", atomic_load(&(((Object *)l)->atomicRefCount)));
#endif
  printf("Releasing the list:\n");
  clock_t ds = clock();
  release(l);
  pd();
  clock_t dp = clock();
  printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
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
  memset(array, 0, size);
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



int main() {
  srand(0);
  initialise_memory();
  //  for(int i=0; i<30; i++) testList(false);
  testList(false);
  ////    ProfilerStart("xx.prof");
  testVector(false);
  testConcurrentMap(false);
  
  //   ProfilerStop();
  //    getchar();
}

