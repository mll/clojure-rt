#include "TestTools.h"
#include <cmocka.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../Ebr.h"
#include "../Keyword.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Var.h"

#define RACE_ITERATIONS 1000000
#define DESTRUCT_ITERATIONS 1000

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
#if defined(__GNUC__) || defined(__clang__)
#define CPU_PAUSE() __asm__ __volatile__("yield")
#else
#define CPU_PAUSE() __builtin_arm_yield()
#endif
#else
#define CPU_PAUSE()                                                            \
  do {                                                                         \
  } while (0)
#endif

static atomic_bool stop_threads = false;

/* --- Test 1: Concurrent bindRoot and deref (The "Race Condition" Test) --- */

struct RaceArgs {
  Var *v;
};

void *writer_thread(void *arg) {
  Ebr_register_thread();
  Ebr_enter_critical();
  struct RaceArgs *args = (struct RaceArgs *)arg;
  for (int i = 0; i < RACE_ITERATIONS && !atomic_load(&stop_threads); i++) {
    RTValue val = RT_boxPtr(String_create("value"));
    Ptr_retain(args->v);
    Var_bindRoot(args->v, val);
    Ebr_flush_critical();
  }
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

void *reader_thread(void *arg) {
  Ebr_register_thread();
  Ebr_enter_critical();
  struct RaceArgs *args = (struct RaceArgs *)arg;
  while (!atomic_load(&stop_threads)) {
    Ptr_retain(args->v);
    RTValue val = Var_deref(args->v);
    if (val != RT_boxNull()) {
      // Accessing the object to trigger ASan if it's freed
      // toString is consuming in this runtime
      Ptr_release(toString(val));
    }
    Ebr_flush_critical();
    CPU_PAUSE();
  }
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

static void test_var_concurrent_bind_deref_race(void **state) {
  (void)state;
  atomic_store(&stop_threads, false);
  RTValue sym = Keyword_create(String_create("race"));
  Var *v = Var_create(sym);
  struct RaceArgs args = {v};

  pthread_t writer, reader;
  pthread_create(&writer, NULL, writer_thread, &args);
  pthread_create(&reader, NULL, reader_thread, &args);

  // Run for a bit to ensure many inter-leavings
  usleep(500000);
  atomic_store(&stop_threads, true);

  pthread_join(writer, NULL);
  pthread_join(reader, NULL);

  Ptr_release(v);
  release(sym);
  Ebr_synchronize_and_reclaim();
  Ebr_synchronize_and_reclaim();
}

/* --- Test 2: Var_destroy synchronization (The "Destruction" Test) --- */
/* In EBR, Var_destroy uses autorelease, so we verify readers are safe */

struct DestructionRaceArgs {
  Var *v;
  _Atomic(long) iteration;   // Writer increments this
  _Atomic(long) reader_done; // Reader increments this
  _Atomic(bool)
      reader_entering; // Reader sets this when it's pinned the pointer
};

void *destruction_writer(void *arg) {
  Ebr_register_thread();
  Ebr_enter_critical();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  Var *v = args->v;

  for (long i = 1; i <= DESTRUCT_ITERATIONS && !atomic_load(&stop_threads);
       i++) {
    Ebr_flush_critical();
    RTValue val = RT_boxPtr(String_create("val"));
    Ptr_retain(v);
    Var_bindRoot(v, val);

    atomic_store(&args->reader_entering, false);
    atomic_store(&args->iteration, i); // Signal reader to start

    // Wait for reader to indicate it is entering critical section
    while (!atomic_load(&args->reader_entering) &&
           !atomic_load(&stop_threads)) {
      CPU_PAUSE();
    }

    if (atomic_load(&stop_threads))
      break;

    // In EBR, Var_destroy doesn't block, it just retires.
    // We want to test that the reader who is in critical section is safe.
    Var_destroy(v);

    // Reset for next iteration (re-init struct members that destroy cleared)
    // Note: This is an artificial test of Var internal state
    atomic_store(&v->root, RT_boxNull());
    v->keyword = Keyword_create(String_create("temp"));

    // Wait for reader to finish its part of the iteration
    while (atomic_load(&args->reader_done) < i && !atomic_load(&stop_threads)) {
      CPU_PAUSE();
      // Periodically reclaim to stress the system
      Ebr_synchronize_and_reclaim();
    }
  }
  atomic_store(&stop_threads, true);
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

void *destruction_reader(void *arg) {
  Ebr_register_thread();
  Ebr_enter_critical();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  Var *v = args->v;
  long current_iter = 0;

  while (!atomic_load(&stop_threads)) {
    Ebr_flush_critical();
    long next_iter = atomic_load(&args->iteration);
    if (next_iter > current_iter) {
      current_iter = next_iter;

      RTValue val = atomic_load_explicit(&v->root, memory_order_acquire);
      if (val != RT_boxNull() && RT_isPtr(val)) {
        // Signal writer that we are in critical section
        atomic_store(&args->reader_entering, true);

        // Targeted delay to maximize window for destruction/reclamation
        for (int j = 0; j < 1000; j++)
          CPU_PAUSE();

        retain(val);
        release(val);
      } else {
        atomic_store(&args->reader_entering, true);
      }

      atomic_store(&args->reader_done, current_iter);
    }
    CPU_PAUSE();
  }
  Ebr_leave_critical();
  Ebr_unregister_thread();
  return NULL;
}

static void test_var_destruction_hazard_race_stable(void **state) {
  (void)state;
  atomic_store(&stop_threads, false);
  RTValue sym = Keyword_create(String_create("stable"));
  Var *v = Var_create(sym);
  struct DestructionRaceArgs args = {
      .v = v, .iteration = 0, .reader_done = 0, .reader_entering = false};

  pthread_t writer, reader;
  pthread_create(&writer, NULL, destruction_writer, &args);
  pthread_create(&reader, NULL, destruction_reader, &args);

  pthread_join(writer, NULL);
  pthread_join(reader, NULL);

  Ptr_release(v);
  release(sym);
  Ebr_synchronize_and_reclaim();
  Ebr_synchronize_and_reclaim();
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_concurrent_bind_deref_race),
      cmocka_unit_test(test_var_destruction_hazard_race_stable),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
