#include "Object.h"
#include "PersistentList.h"
#include "Integer.h"
#include <time.h>

void testList (bool pauses) {
  initialise_memory();
  printf("Total size: %lu %lu\n", sizeof(Object), sizeof(Integer)); 
  PersistentList *l = PersistentList_create(NULL, NULL);
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  printf("Press a key to start\n");
  if(pauses) getchar();
  clock_t as = clock();

  for (int i=0;i<100000000; i++) {
    Integer *n = Integer_create(i);
    PersistentList *k = PersistentList_conj(l, super(n));
    release(n);
    release(l);
    l = k;
  }
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
  clock_t dp = clock();
  printf("Released\nTime: %f\n", (double)(dp - ds) / CLOCKS_PER_SEC);
}


int main() {
  for(int i=0; i<30; i++) testList(false);
  getchar();
}

