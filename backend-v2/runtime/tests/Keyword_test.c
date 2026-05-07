#include "TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <pthread.h>
#include "../Keyword.h"
#include "../PersistentVector.h"
#include "../RuntimeInterface.h"
#include "../String.h"

static void test_keyword_interning(void **state) {
  (void)state; // unused

  // This test proves that the memory balancing allows unique string allocations
  // introduced by newly interned keywords (which leak into the hash map
  // globally)
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("my-unique-test-key");
    RTValue kw = Keyword_create(s);

    PersistentVector *v = PersistentVector_create();
    v = PersistentVector_conj(v, kw);

    Ptr_release(v);
  });
}

#define THREAD_COUNT 20
#define ITERATIONS 500

typedef struct KeywordThreadParams {
  const char *name;
  RTValue results[ITERATIONS];
} KeywordThreadParams;

static void *keywordThread(void *param) {
  KeywordThreadParams *p = (KeywordThreadParams *)param;
  for (int i = 0; i < ITERATIONS; i++) {
    p->results[i] = Keyword_create(String_create(p->name));
  }
  return NULL;
}

static void test_concurrent_keyword_interning(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    KeywordThreadParams params[THREAD_COUNT];
    pthread_t threads[THREAD_COUNT];
    const char *shared_name = "concurrent-key";

    for (int i = 0; i < THREAD_COUNT; i++) {
      params[i].name = shared_name;
      pthread_create(&threads[i], NULL, keywordThread, &params[i]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
      pthread_join(threads[i], NULL);
    }

    RTValue first = params[0].results[0];
    assert_true(RT_isKeyword(first));

    for (int i = 0; i < THREAD_COUNT; i++) {
      for (int j = 0; j < ITERATIONS; j++) {
        assert_int_equal(RT_unboxKeyword(params[i].results[j]),
                         RT_unboxKeyword(first));
      }
    }
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_keyword_interning),
      cmocka_unit_test(test_concurrent_keyword_interning),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
