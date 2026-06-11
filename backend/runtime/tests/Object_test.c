#include "TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../Object.h"
#include "../RuntimeInterface.h"
#include "../String.h"
#include "../PersistentList.h"
#include "runtime/ObjectProto.h"
#include "runtime/RTValue.h"

// Test that allocating an object gives a refcount of COUNT_INC and it's not shared.
static void test_initial_refcount(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("test");
    Object *obj = (Object *)s;
    uword_t raw = Object_getRawRefCount(obj);
    assert_int_equal(raw, COUNT_INC);
    assert_false(raw & SHARED_BIT);
    Ptr_release(s);
  });
}

// Test that local retain and release work as expected
static void test_local_retain_release(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("test");
    Object *obj = (Object *)s;
    
    Object_retain(obj);
    uword_t raw = Object_getRawRefCount(obj);
    assert_int_equal(raw, COUNT_INC * 2);
    assert_false(raw & SHARED_BIT);
    
    bool released = Object_release(obj);
    assert_false(released); // Not deallocated yet
    raw = Object_getRawRefCount(obj);
    assert_int_equal(raw, COUNT_INC);
    
    released = Object_release(obj);
    assert_true(released); // Deallocated
  });
}

// Test promotion to shared and shared retain/release
static void test_shared_promotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("test");
    Object *obj = (Object *)s;
    
    Object_promoteToShared(obj);
    uword_t raw = Object_getRawRefCount(obj);
    // It should now have the shared bit set
    assert_true(raw & SHARED_BIT);
    assert_int_equal(raw >> 1, 1);
    
    Object_retain(obj);
    raw = Object_getRawRefCount(obj);
    assert_true(raw & SHARED_BIT);
    assert_int_equal(raw >> 1, 2);
    
    bool released = Object_release(obj);
    assert_false(released);
    raw = Object_getRawRefCount(obj);
    assert_true(raw & SHARED_BIT);
    assert_int_equal(raw >> 1, 1);
    
    released = Object_release(obj);
    assert_true(released);
  });
}

// Test Object_isReusable
static void test_is_reusable(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("test");
    Object *obj = (Object *)s;
    
    // A newly created object with refcount 1 is reusable
    assert_true(Object_isReusable(obj));
    
    // If we retain it, it's no longer reusable
    Object_retain(obj);
    assert_false(Object_isReusable(obj));
    Object_release(obj);
    
    // If we promote it to shared, and it has count 1, it is reusable
    // AND calling isReusable should drop the shared bit!
    Object_promoteToShared(obj);
    uword_t raw = Object_getRawRefCount(obj);
    assert_true(raw & SHARED_BIT);
    
    assert_true(Object_isReusable(obj));
    raw = Object_getRawRefCount(obj);
    assert_false(raw & SHARED_BIT); // should be de-promoted
    
    Ptr_release(s);
  });
}

// Test recursive promotion
static void test_recursive_promotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_create("hello");
    String *s2 = String_create("world");
    
    PersistentVector *vec = PersistentVector_create();
    vec = PersistentVector_conj(vec, RT_boxPtr(s1));
    vec = PersistentVector_conj(vec, RT_boxPtr(s2));
    
    Object *vecObj = (Object *)vec;
    Object *s1Obj = (Object *)s1;
    Object *s2Obj = (Object *)s2;
    
    // Initially none are shared
    assert_false(Object_getRawRefCount(vecObj) & SHARED_BIT);
    assert_false(Object_getRawRefCount(s1Obj) & SHARED_BIT);
    assert_false(Object_getRawRefCount(s2Obj) & SHARED_BIT);
    
    // Promote the vector
    Object_promoteToShared(vecObj);
    
    // Now the vector and its elements should be shared
    assert_true(Object_getRawRefCount(vecObj) & SHARED_BIT);
    assert_true(Object_getRawRefCount(s1Obj) & SHARED_BIT);
    assert_true(Object_getRawRefCount(s2Obj) & SHARED_BIT);
    
    Ptr_release(vec);
  });
}

#define STRESS_THREADS 8
#define STRESS_ITERATIONS 100000

struct StressArgs {
  Object *obj;
};

void *stress_thread(void *arg) {
  struct StressArgs *args = (struct StressArgs *)arg;
  Object *obj = args->obj;
  for (int i = 0; i < STRESS_ITERATIONS; i++) {
    Object_retain(obj);
    Object_release(obj);
  }
  return NULL;
}

static void test_concurrent_refcounting(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("stress_target");
    Object *obj = (Object *)s;
    
    // Promote to shared before starting threads
    Object_promoteToShared(obj);
    
    uword_t initial_raw = Object_getRawRefCount(obj);
    
    pthread_t threads[STRESS_THREADS];
    struct StressArgs args = { obj };
    
    for (int i = 0; i < STRESS_THREADS; i++) {
      pthread_create(&threads[i], NULL, stress_thread, &args);
    }
    
    for (int i = 0; i < STRESS_THREADS; i++) {
      pthread_join(threads[i], NULL);
    }
    
    uword_t final_raw = Object_getRawRefCount(obj);
    assert_int_equal(initial_raw, final_raw);
    
    Ptr_release(s);
  });
}

static void test_concurrent_is_reusable_depromotion(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s = String_create("depromote_target");
    Object *obj = (Object *)s;
    
    // Promote to shared before starting threads
    Object_promoteToShared(obj);
    
    pthread_t threads[STRESS_THREADS];
    struct StressArgs args = { obj };
    
    for (int i = 0; i < STRESS_THREADS; i++) {
      pthread_create(&threads[i], NULL, stress_thread, &args);
    }
    
    for (int i = 0; i < STRESS_THREADS; i++) {
      pthread_join(threads[i], NULL);
    }
    
    // Now it should be back to 1 refcount and shared
    uword_t raw = Object_getRawRefCount(obj);
    assert_true(raw & SHARED_BIT);
    assert_int_equal(raw >> 1, 1);
    
    // Now checking if reusable should return true AND de-promote it!
    assert_true(Object_isReusable(obj));
    raw = Object_getRawRefCount(obj);
    assert_false(raw & SHARED_BIT); // Shared bit should be dropped
    assert_int_equal(raw, COUNT_INC); // Refcount should be exactly 1 (which is COUNT_INC)
    
    Ptr_release(s);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_initial_refcount),
      cmocka_unit_test(test_local_retain_release),
      cmocka_unit_test(test_shared_promotion),
      cmocka_unit_test(test_is_reusable),
      cmocka_unit_test(test_recursive_promotion),
      cmocka_unit_test(test_concurrent_refcounting),
      cmocka_unit_test(test_concurrent_is_reusable_depromotion),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
