#include "TestTools.h"

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

int main() {
  srand(0);
  initialise_memory();
//  testConcurrentMap(false);
  return 0;  
}
