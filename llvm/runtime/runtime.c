#include "Object.h"
#include "PersistentList.h"
#include "Integer.h"
#include <time.h>

void testList () {
  PersistentList *l = PersistentList_create(NULL, NULL);
  // l = l->conj(new Number(3));
  // l = l->conj(new Number(7));
  // l = l->conj(new PersistentList(new Number(2)));
  // l = l->conj(new PersistentList());
  printf("Press a key to start\n");
  getchar();
  clock_t as = clock();

  for (int i=0;i<100000000; i++) {
    Integer *n = Integer_create(i);
    PersistentList *k = PersistentList_conj(l, n->super);
    release(n->super);
    release(l->super);
    l = k;
  }
  clock_t ap = clock();
  printf("Array size: %llu\nRef count: %llu\nTime: %f\n", l->count, l->super->refCount, (double)(ap - as) / CLOCKS_PER_SEC);
   
  getchar();
  clock_t os = clock();

  PersistentList *tmp = l;
  int64_t sum = 0;
  while(tmp != NULL) {
    if(tmp->first) sum += ((Integer *)(tmp->first->data))->value;
    tmp = tmp->rest;
  }
  clock_t op = clock();
  printf("Sum: %llu\nTime: %f", sum, (double)(op - os) / CLOCKS_PER_SEC);
  getchar();
  clock_t ds = clock();
  release(l->super);
  clock_t dp = clock();
  printf("Released\n%llu\nTime: %f\n", l->super->refCount, (double)(dp - ds) / CLOCKS_PER_SEC);

}


int main() {
  testList();

}

