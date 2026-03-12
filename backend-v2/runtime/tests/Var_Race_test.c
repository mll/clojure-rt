#include "TestTools.h"
#include <cmocka.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../Keyword.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Var.h"
#include "TestTools.h"

#define ITERATIONS 2000000

static atomic_bool stop_threads = false;

struct ThreadArgs {
  Var *v;
};

void *writer_thread(void *arg) {
  Var_thread_initialize();
  struct ThreadArgs *args = (struct ThreadArgs *)arg;
  Var *v = args->v;
  Ptr_retain(v);

  int i = 0;
  for (i = 0; i < ITERATIONS && !atomic_load(&stop_threads); ++i) {
    // Create a new string and bind it as root
    RTValue val = RT_boxPtr(String_create("new-value"));
    Ptr_retain(v);
    Var_bindRoot(v, val);
    // Var_bindRoot releases the old value, and it was the only owner.
    // If the reader thread was just about to retain it, we have a UAF.
  }
  Ptr_release(v);
  printf("!!! Terminating writer thread after %d iterations\n", i);
  atomic_store(&stop_threads, true);
  return NULL;
}

void *reader_thread(void *arg) {
  Var_thread_initialize();
  struct ThreadArgs *args = (struct ThreadArgs *)arg;
  Var *v = args->v;
  Ptr_retain(v);
  int i = 0;
  for (i = 0; i < ITERATIONS && !atomic_load(&stop_threads); ++i) {
    // Deref the var. This reads v->root and then retains it.
    // There is no lock between reading and retaining.
    Ptr_retain(v);
    RTValue val = Var_deref(v);

    // Use the value to trigger a crash if it was freed
    if (RT_isPtr(val)) {
      String *s = (String *)RT_unboxPtr(val);
      // This might crash if s was already freed and its memory reused/poisoned
      const char *c_str = String_c_str(s);
      (void)c_str;
    }

    release(val);
  }
  Ptr_release(v);
  printf("!!! Terminating reader thread after %d iterations\n", i);
  atomic_store(&stop_threads, true);
  return NULL;
}

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#define CPU_PAUSE() __asm__ __volatile__("yield")
#else
#define CPU_PAUSE() \
  do { \
  } while (0)
#endif

struct DestructionRaceArgs {
  _Atomic(Var *) v_ptr;
};

void *destruction_writer(void *arg) {
  Var_thread_initialize();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  for (int i = 0; i < ITERATIONS && !atomic_load(&stop_threads); i++) {
    RTValue sym = Keyword_create(String_create("temp"));
    Var *v = Var_create(sym);
    Ptr_retain(v); // Survive Var_bindRoot
    Var_bindRoot(v, RT_boxPtr(String_create("val")));

    // We give one reference to the reader
    Ptr_retain(v);
    atomic_store(&args->v_ptr, v);

    // Final release of our reference
    Ptr_release(v);
    release(sym);

    // The reader should now have the only reference.
    // We wait for reader to consume it.
    while (atomic_load(&args->v_ptr) != NULL && !atomic_load(&stop_threads)) {
      CPU_PAUSE();
    }
  }
  printf("!!! Terminating destruction writer thread\n");
  atomic_store(&stop_threads, true);
  return NULL;
}

void *destruction_reader(void *arg) {
  Var_thread_initialize();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  int i = 0;
  while (!atomic_load(&stop_threads)) {
    Var *v = atomic_exchange(&args->v_ptr, NULL);
    if (v) {
      // Var_deref consumes v
      RTValue val = Var_deref(v);
      release(val);
      i++;
    }
  }
  printf("!!! Terminating destruction reader thread after %d iterations\n", i);
  return NULL;
}

static void test_var_destruction_hazard_race(void **state) {
  (void)state;
  atomic_store(&stop_threads, false);
  struct DestructionRaceArgs args;
  atomic_store(&args.v_ptr, NULL);

  pthread_t writer, reader;
  pthread_create(&writer, NULL, destruction_writer, &args);
  pthread_create(&reader, NULL, destruction_reader, &args);

  pthread_join(writer, NULL);
  pthread_join(reader, NULL);
}

static void test_var_race_condition(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue sym = Keyword_create(String_create("race-var"));
    Var *v = Var_create(sym);

    // Initial binding
    Ptr_retain(v);
    Var_bindRoot(v, RT_boxPtr(String_create("initial-value")));

    struct ThreadArgs args = {.v = v};
    pthread_t writer;
    pthread_t reader;

    pthread_create(&writer, NULL, writer_thread, &args);
    pthread_create(&reader, NULL, reader_thread, &args);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    // If we reached here without a crash, the race didn't trigger in this run.
    // In a real scenario, we might want to run this in a loop or with ASan.

    // Cleanup (might also crash if memory is corrupted)
    Var_unbindRoot(v);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_race_condition),
      cmocka_unit_test(test_var_destruction_hazard_race),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
