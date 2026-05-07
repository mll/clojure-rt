#include "../Object.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include <stdio.h>
#include <time.h>

#define ITERATIONS 100000000

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
  initialise_memory();
  RuntimeInterface_initialise();
  printf("Starting refcount performance benchmark with %d iterations...\n",
         ITERATIONS);

  String *s = String_create("benchmark");
  Object *obj = (Object *)s;

  // Test Local
  double start_local = get_time();
  for (int i = 0; i < ITERATIONS; i++) {
    Object_retain(obj);
    __asm__ volatile("" : "+r"(obj));
    Object_release(obj);
    __asm__ volatile("" : "+r"(obj));
  }
  double end_local = get_time();
  double local_duration = end_local - start_local;
  printf("Local Path (relaxed): %f seconds\n", local_duration);

  // Promote to shared
  Object_promoteToShared(obj);
  printf("Object promoted to shared.\n");

  // Test Shared
  double start_shared = get_time();
  for (int i = 0; i < ITERATIONS; i++) {
    Object_retain(obj);
    __asm__ volatile("" : "+r"(obj));
    Object_release(obj);
    __asm__ volatile("" : "+r"(obj));
  }
  double end_shared = get_time();
  double shared_duration = end_shared - start_shared;
  printf("Shared Path (atomic):  %f seconds\n", shared_duration);

  if (local_duration > 0) {
    printf("Performance improvement: %.2fx faster\n",
           shared_duration / local_duration);
  }

  Ptr_release(s);
  RuntimeInterface_cleanup();
  return 0;
}
