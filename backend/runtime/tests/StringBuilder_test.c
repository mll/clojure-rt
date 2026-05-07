#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "../Object.h"
#include "../String.h"
#include "../StringBuilder.h"
#include "TestTools.h"

static void testStringBuilderBasic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("Hello");
    StringBuilder *sb = StringBuilder_create(s1);

    assert_int_equal(getType(RT_boxPtr(sb)), stringBuilderType);
    
    // toString should return "Hello" and release sb
    String *res = StringBuilder_toString(sb);
    assert_true(String_equals(res, s1));
    
    Ptr_release(res);
  });
}

static void testStringBuilderAppend(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("Hello");
    String *s2 = String_createStatic(" World");
    StringBuilder *sb = StringBuilder_create(s1);

    // append World
    sb = StringBuilder_append(sb, s2);
    
    String *res = StringBuilder_toString(sb);
    String *expected = String_createStatic("Hello World");
    
    assert_true(String_equals(res, expected));
    
    Ptr_release(res);
    Ptr_release(expected);
  });
}

static void testStringBuilderMultipleAppend(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    StringBuilder *sb = StringBuilder_create(String_createStatic("A"));
    sb = StringBuilder_append(sb, String_createStatic("B"));
    sb = StringBuilder_append(sb, String_createStatic("C"));
    sb = StringBuilder_append(sb, String_createStatic("D"));

    String *res = StringBuilder_toString(sb);
    String *expected = String_createStatic("ABCD");
    
    assert_true(String_equals(res, expected));
    
    Ptr_release(res);
    Ptr_release(expected);
  });
}

static void testStringBuilderEqualsAndHash(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    String *s1 = String_createStatic("test");
    String *s2 = String_createStatic("test");
    StringBuilder *sb1 = StringBuilder_create(s1);
    StringBuilder *sb2 = StringBuilder_create(s2);

    assert_true(StringBuilder_equals(sb1, sb2));
    assert_int_equal(StringBuilder_hash(sb1), StringBuilder_hash(sb2));
    
    // Also test through Object methods
    assert_true(Object_equals((Object*)sb1, (Object*)sb2));
    assert_int_equal(Object_hash((Object*)sb1), Object_hash((Object*)sb2));

    Ptr_release(sb1);
    Ptr_release(sb2);
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(testStringBuilderBasic),
      cmocka_unit_test(testStringBuilderAppend),
      cmocka_unit_test(testStringBuilderMultipleAppend),
      cmocka_unit_test(testStringBuilderEqualsAndHash),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  srand(0);
  return cmocka_run_group_tests(tests, NULL, NULL);
}
