#include "TestTools.h"
#include <cmocka.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>

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
#include <arm_acle.h>
#define CPU_PAUSE() __asm__ __volatile__("yield")
#else
#define CPU_PAUSE()                                                            \
  do {                                                                         \
  } while (0)
#endif

static atomic_bool stop_threads = false;

typedef struct HazardSlot {
  _Atomic(RTValue) hazardPointer;
  _Atomic(bool) active;
  struct HazardSlot *next;
} HazardSlot;

extern HazardSlot *getOrCreateSlot(void);
extern void asymmetric_barrier(void);
extern _Atomic(HazardSlot *) hazardHead;
extern void Var_destroy(Var *self);

/* --- Test 1: Concurrent bindRoot and deref (The "Race Condition" Test) --- */

struct RaceArgs {
  Var *v;
};

void *writer_thread(void *arg) {
  Var_thread_initialize();
  struct RaceArgs *args = (struct RaceArgs *)arg;
  for (int i = 0; i < RACE_ITERATIONS && !atomic_load(&stop_threads); i++) {
    RTValue val = RT_boxPtr(String_create("value"));
    Ptr_retain(args->v);
    Var_bindRoot(args->v, val);
  }
  return NULL;
}

void *reader_thread(void *arg) {
  Var_thread_initialize();
  struct RaceArgs *args = (struct RaceArgs *)arg;
  while (!atomic_load(&stop_threads)) {
    Ptr_retain(args->v);
    RTValue val = Var_deref(args->v);
    if (val != RT_boxNull()) {
      // Accessing the object to trigger ASan if it's freed
      // toString is consuming in this runtime
      toString(val);
    }
    CPU_PAUSE();
  }
  return NULL;
}

static void test_var_concurrent_bind_deref_race(void **state) {
  (void)state;
  atomic_store(&stop_threads, false);
  Var *v = Var_create(Keyword_create(String_create("race")));
  struct RaceArgs args = {v};

  pthread_t writer, reader;
  pthread_create(&writer, NULL, writer_thread, &args);
  pthread_create(&reader, NULL, reader_thread, &args);

  // Run for a bit to ensure many inter-leavings
  usleep(500000);
  atomic_store(&stop_threads, true);

  pthread_join(writer, NULL);
  pthread_join(reader, NULL);
}

/* --- Test 2: Var_destroy synchronization (The "Destruction" Test) --- */

struct DestructionRaceArgs {
  Var *v;
  _Atomic(long) iteration;   // Writer increments this
  _Atomic(long) reader_done; // Reader increments this
  _Atomic(bool)
      reader_entering; // Reader sets this when it's pinned the pointer
};

void *destruction_writer(void *arg) {
  Var_thread_initialize();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  Var *v = args->v;

  for (long i = 1; i <= DESTRUCT_ITERATIONS && !atomic_load(&stop_threads);
       i++) {
    RTValue val = RT_boxPtr(String_create("val"));
    Ptr_retain(v);
    Var_bindRoot(v, val);

    atomic_store(&args->reader_entering, false);
    atomic_store(&args->iteration, i); // Signal reader to start

    // Wait for reader to grab and pin the root
    while (!atomic_load(&args->reader_entering) &&
           !atomic_load(&stop_threads)) {
      CPU_PAUSE();
    }

    if (atomic_load(&stop_threads))
      break;

    // Call Var_destroy manually.
    // This MUST wait for the reader who has 'val' pinned in hazard pointer.
    Var_destroy(v);

    // Reset for next iteration (re-init struct members that destroy cleared)
    atomic_store(&v->root, RT_boxNull());
    v->keyword = Keyword_create(String_create("temp"));

    // Wait for reader to finish its part of the iteration
    while (atomic_load(&args->reader_done) < i && !atomic_load(&stop_threads)) {
      CPU_PAUSE();
    }
  }
  atomic_store(&stop_threads, true);
  return NULL;
}

void *destruction_reader(void *arg) {
  Var_thread_initialize();
  struct DestructionRaceArgs *args = (struct DestructionRaceArgs *)arg;
  Var *v = args->v;
  long current_iter = 0;

  while (!atomic_load(&stop_threads)) {
    long next_iter = atomic_load(&args->iteration);
    if (next_iter > current_iter) {
      current_iter = next_iter;

      RTValue val = atomic_load_explicit(&v->root, memory_order_acquire);
      if (val != RT_boxNull() && RT_isPtr(val)) {
        HazardSlot *slot = getOrCreateSlot();

        // Advertise
        atomic_store_explicit(&slot->hazardPointer, val, memory_order_relaxed);
        atomic_signal_fence(memory_order_seq_cst);

        // Double check
        if (val == atomic_load_explicit(&v->root, memory_order_acquire)) {
          // Signal writer that we are in
          atomic_store(&args->reader_entering, true);

          // Targeted delay: right before retain, maximize window for
          // destruction
          for (int j = 0; j < 1000; j++)
            CPU_PAUSE();

          retain(val);
          release(val);
        } else {
          // If we missed it, signal so writer doesn't hang
          atomic_store(&args->reader_entering, true);
        }

        // Clear advertisement IMMEDIATELY so writer can proceed
        atomic_store_explicit(&slot->hazardPointer, RT_boxNull(),
                              memory_order_relaxed);
      } else {
        // Fallback to avoid hanging writer if root wasn't visible
        atomic_store(&args->reader_entering, true);
      }

      atomic_store(&args->reader_done, current_iter);
    }
    CPU_PAUSE();
  }
  return NULL;
}

static void test_var_destruction_hazard_race_stable(void **state) {
  (void)state;
  atomic_store(&stop_threads, false);
  Var *v = Var_create(Keyword_create(String_create("stable")));
  Ptr_retain(v);

  struct DestructionRaceArgs args = {
      .v = v, .iteration = 0, .reader_done = 0, .reader_entering = false};

  pthread_t writer, reader;
  pthread_create(&writer, NULL, destruction_writer, &args);
  pthread_create(&reader, NULL, destruction_reader, &args);

  pthread_join(writer, NULL);
  pthread_join(reader, NULL);

  Ptr_release(v);
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_concurrent_bind_deref_race),
      cmocka_unit_test(test_var_destruction_hazard_race_stable),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
