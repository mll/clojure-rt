#include "TestTools.h"
#include "../Class.h"
#include <cmocka.h>

static void test_class_create_destroy(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    // Let's use dynamic strings to be safe with retain counts
    String *dynName = String_createDynamicStr("test.MyClass");
    String *dynClassName = String_createDynamicStr("test.MyClass");
    
    // Object_create inside Class_create gives self a refcount of 1
    Class *cls = Class_create(dynName, dynClassName, 0, NULL);
    assert_non_null(cls);
    assert_false(cls->isProtocol);

    Ptr_release(cls);
  });
}

static void test_class_protocol(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *dynName = String_createDynamicStr("test.MyProto");
    String *dynClassName = String_createDynamicStr("test.MyProto");
    
    Class *cls = Class_createProtocol(dynName, dynClassName, 0, NULL);
    assert_non_null(cls);
    assert_true(cls->isProtocol);

    Ptr_release(cls);
  });
}

static void test_class_tostring(void **state) {
  ASSERT_MEMORY_ALL_BALANCED({
    String *dynName = String_createDynamicStr("test.MyClass");
    String *dynClassName = String_createDynamicStr("test.MyClass");
    
    Class *cls = Class_create(dynName, dynClassName, 0, NULL);
    
    Ptr_retain(cls);
    String *s = Class_toString(cls);
    assert_string_equal(String_c_str(s), "test.MyClass");
    
    Ptr_release(s);
    Ptr_release(cls);
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_class_create_destroy),
      cmocka_unit_test(test_class_protocol),
      cmocka_unit_test(test_class_tostring),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
