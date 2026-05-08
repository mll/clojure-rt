#include "TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "../Keyword.h"
#include "../PersistentVector.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "runtime/Ebr.h"
#include <pthread.h>

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
#include <stdio.h>

#define THREAD_COUNT 4
#define ITERATIONS 50

typedef struct KeywordThreadParams {
  const char *name;
  RTValue results[ITERATIONS];
} KeywordThreadParams;

static void *keywordThread(void *param) {
  Ebr_register_thread();
  KeywordThreadParams *p = (KeywordThreadParams *)param;
  for (int i = 0; i < ITERATIONS; i++) {
    char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "%s-%d", p->name, i);
    p->results[i] = Keyword_create(String_createDynamicStr(name_buf));
  }
  Ebr_unregister_thread();
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

    for (int j = 0; j < ITERATIONS; j++) {
      RTValue expected = params[0].results[j];
      assert_true(RT_isKeyword(expected));
      for (int i = 0; i < THREAD_COUNT; i++) {
        assert_int_equal(RT_unboxKeyword(params[i].results[j]),
                         RT_unboxKeyword(expected));
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
