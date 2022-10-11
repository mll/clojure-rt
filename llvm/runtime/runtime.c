#include "Object.h"
#include "PersistentList.h"
#include "PersistentVector.h"
#include "PersistentVectorNode.h"
#include "Integer.h"
#include <time.h>
#include <stdio.h>

extern int allocationCount[5];

void pd() {
    printf("Ref counters: %d %d %d %d %d\n", allocationCount[0], allocationCount[1], allocationCount[2], allocationCount[3], allocationCount[4]);
}

void testList (bool pauses) {
  printf("Total size: %lu %lu\n", sizeof(Object), sizeof(Integer)); 
  PersistentList *l = PersistentList_create(NULL, NULL);
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  printf("Press a key to start\n");
  pd();
  if(pauses) getchar();
  clock_t as = clock();

  for (int i=0;i<100000000; i++) {
    Integer *n = Integer_create(i);
    PersistentList *k = PersistentList_conj(l, super(n));
    release(n);
    release(l);
    l = k;
  }
  pd();
  clock_t ap = clock();
  printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, super(l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
   
  if (pauses) getchar();
  clock_t os = clock();

  PersistentList *tmp = l;
  int64_t sum = 0;
  while(tmp != NULL) {
    if(tmp->first) sum += ((Integer *)(data(tmp->first)))->value;
    tmp = tmp->rest;
  }
  clock_t op = clock();
  printf("Sum: %llu\nTime: %f\n", sum, (double)(op - os) / CLOCKS_PER_SEC);
  if(pauses) getchar();
  clock_t ds = clock();
  printf("%llu\n", super(l)->refCount);
  release(l);
  pd();
  clock_t dp = clock();
  printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
}


void testVector (bool pauses) {
  printf("Total size: %lu %lu\n", sizeof(Object), sizeof(Integer)); 
  printf("Total size: %lu %lu\n", sizeof(PersistentVector), sizeof(PersistentVectorNode)); 
  PersistentVector *l = PersistentVector_create();
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  printf("Press a key to start\n");
  pd();
  if(pauses) getchar();
  clock_t as = clock();

  for (int i=0;i<100000000; i++) {
   // PersistentVector_print(l);
   // printf("=======*****************===========");
   // fflush(stdout);
    Integer *n = Integer_create(i);
    PersistentVector *k = PersistentVector_conj(l, super(n));
    release(n);
    release(l);
    l = k;
//    printf("%d\r",i);
  }
  pd();
  clock_t ap = clock();
  printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, super(l)->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
   
  if (pauses) getchar();
  clock_t os = clock();
  
  int64_t sum = 0;
  for(int i=0; i< l->count; i++) {
    sum += ((Integer *) data(PersistentVector_nth(l, i)))->value;
  }
  clock_t op = clock();
  printf("Sum: %llu\nTime: %f\n", sum, (double)(op - os) / CLOCKS_PER_SEC);
  if(pauses) getchar();
  clock_t ds = clock();
  printf("%llu\n", super(l)->refCount);
  release(l);
  pd();
  clock_t dp = clock();
  printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
}



int main() {
    initialise_memory();
//  for(int i=0; i<30; i++) testList(false);
    testList(false);
    testVector(false);
    getchar();
    pd();
}

