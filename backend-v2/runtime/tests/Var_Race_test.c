#include "TestTools.h"
#include <cmocka.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../Keyword.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../Var.h"
#include "TestTools.h"

#define ITERATIONS 1000000

static atomic_bool stop_threads = false;

struct ThreadArgs {
  Var *v;
};

void *writer_thread(void *arg) {
  struct ThreadArgs *args = (struct ThreadArgs *)arg;
  Var *v = args->v;
  Ptr_retain(v);

  for (int i = 0; i < ITERATIONS && !atomic_load(&stop_threads); ++i) {
    // Create a new string and bind it as root
    RTValue val = RT_boxPtr(String_create("new-value"));
    Ptr_retain(v);
    Var_bindRoot(v, val);
    // Var_bindRoot releases the old value, and it was the only owner.
    // If the reader thread was just about to retain it, we have a UAF.
  }
  Ptr_release(v);
  atomic_store(&stop_threads, true);
  return NULL;
}

void *reader_thread(void *arg) {
  struct ThreadArgs *args = (struct ThreadArgs *)arg;
  Var *v = args->v;
  Ptr_retain(v);
  for (int i = 0; i < ITERATIONS && !atomic_load(&stop_threads); ++i) {
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
  atomic_store(&stop_threads, true);
  return NULL;
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
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
