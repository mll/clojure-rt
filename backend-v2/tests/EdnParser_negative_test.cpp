#include <iostream>
#include <string>
#include <vector>

#include "../RuntimeHeaders.h"
#include "../tools/EdnParser.h"

extern "C" {
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "../runtime/Keyword.h"
#include "../runtime/PersistentArrayMap.h"
#include "../runtime/PersistentVector.h"
#include "../runtime/RuntimeInterface.h"
#include "../runtime/String.h"
#include "../runtime/Symbol.h"
#include "../runtime/tests/TestTools.h"
}

using namespace std;
using namespace rt;

static void run_negative_test(void (*test_func)(void **), void **state,
                              const char *name) {
  try {
    test_func(state);
    fprintf(stderr, "Test %s failed: Expected exception not thrown\n", name);
    assert_true(false);
  } catch (const LanguageException &e) {
    // Expected
  } catch (...) {
    fprintf(stderr, "Test %s failed with unknown exception\n", name);
    assert_true(false);
  }
}

// --- Group 1: TemporaryClassData (Root Parsing) ---

static void test_root_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({ buildClasses(RT_boxInt32(42)); });
}

static void test_root_not_map(void **state) {
  run_negative_test(test_root_not_map_impl, state, "test_root_not_map");
}

static void test_class_key_not_symbol_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(rootMap, RT_boxInt32(1),
                                       RT_boxPtr(PersistentArrayMap_empty()));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_class_key_not_symbol(void **state) {
  run_negative_test(test_class_key_not_symbol_impl, state,
                    "test_class_key_not_symbol");
}

static void test_class_value_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxInt32(1));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_class_value_not_map(void **state) {
  run_negative_test(test_class_value_not_map_impl, state,
                    "test_class_value_not_map");
}

static void test_object_type_not_int_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("object-type")),
        RT_boxPtr(String_create("100")));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_object_type_not_int(void **state) {
  run_negative_test(test_object_type_not_int_impl, state,
                    "test_object_type_not_int");
}

static void test_alias_not_keyword_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("alias")),
        Symbol_create(String_create("a-symbol")));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_alias_not_keyword(void **state) {
  run_negative_test(test_alias_not_keyword_impl, state,
                    "test_alias_not_keyword");
}

// --- Group 2: ClassDescription ---

static void test_static_fields_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(classMap,
                                        Keyword_create(String_create("name")),
                                        RT_boxPtr(String_create("A")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxInt32(1));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_static_fields_not_map(void **state) {
  run_negative_test(test_static_fields_not_map_impl, state,
                    "test_static_fields_not_map");
}

static void test_instance_fns_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(classMap,
                                        Keyword_create(String_create("name")),
                                        RT_boxPtr(String_create("A")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxInt32(1));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_instance_fns_not_map(void **state) {
  run_negative_test(test_instance_fns_not_map_impl, state,
                    "test_instance_fns_not_map");
}

// --- Group 3: Intrinsic Collections ---

static void test_static_field_key_not_symbol_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *fieldsMap = PersistentArrayMap_empty();
    fieldsMap =
        PersistentArrayMap_assoc(fieldsMap, RT_boxInt32(1), RT_boxInt32(42));
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(classMap,
                                        Keyword_create(String_create("name")),
                                        RT_boxPtr(String_create("A")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxPtr(fieldsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_static_field_key_not_symbol(void **state) {
  run_negative_test(test_static_field_key_not_symbol_impl, state,
                    "test_static_field_key_not_symbol");
}

static void test_intrinsic_key_not_symbol_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, RT_boxInt32(1),
                                      RT_boxPtr(PersistentVector_create()));
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(classMap,
                                        Keyword_create(String_create("name")),
                                        RT_boxPtr(String_create("A")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_key_not_symbol(void **state) {
  run_negative_test(test_intrinsic_key_not_symbol_impl, state,
                    "test_intrinsic_key_not_symbol");
}

static void test_intrinsic_value_not_vector_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxInt32(42));
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(classMap,
                                        Keyword_create(String_create("name")),
                                        RT_boxPtr(String_create("A")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_value_not_vector(void **state) {
  run_negative_test(test_intrinsic_value_not_vector_impl, state,
                    "test_intrinsic_value_not_vector");
}

// --- Group 4: IntrinsicDescription ---

static void test_intrinsic_type_not_keyword_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        RT_boxPtr(String_create("call")));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_type_not_keyword(void **state) {
  run_negative_test(test_intrinsic_type_not_keyword_impl, state,
                    "test_intrinsic_type_not_keyword");
}

static void test_intrinsic_type_invalid_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        Keyword_create(String_create("invalid")));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_type_invalid(void **state) {
  run_negative_test(test_intrinsic_type_invalid_impl, state,
                    "test_intrinsic_type_invalid");
}

static void test_intrinsic_symbol_not_string_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("symbol")),
        Keyword_create(String_create("not-a-string")));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_symbol_not_string(void **state) {
  run_negative_test(test_intrinsic_symbol_not_string_impl, state,
                    "test_intrinsic_symbol_not_string");
}

static void test_intrinsic_args_not_vector_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("sym")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("args")), RT_boxInt32(42));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_args_not_vector(void **state) {
  run_negative_test(test_intrinsic_args_not_vector_impl, state,
                    "test_intrinsic_args_not_vector");
}

static void test_unknown_arg_type_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *argsVec = PersistentVector_create();
    argsVec = PersistentVector_conj(
        argsVec, Keyword_create(String_create("unknown-alias")));

    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("sym")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("args")),
        RT_boxPtr(argsVec));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_unknown_arg_type(void **state) {
  run_negative_test(test_unknown_arg_type_impl, state, "test_unknown_arg_type");
}

static void test_unknown_return_type_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *intrinsicMap = PersistentArrayMap_empty();
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("sym")));
    intrinsicMap = PersistentArrayMap_assoc(
        intrinsicMap, Keyword_create(String_create("returns")),
        Keyword_create(String_create("unknown-alias")));

    PersistentVector *overloads = PersistentVector_create();
    overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsicMap));

    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxPtr(overloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_unknown_return_type(void **state) {
  run_negative_test(test_unknown_return_type_impl, state,
                    "test_unknown_return_type");
}

int main(void) {
  initialise_memory();
  PersistentArrayMap_empty();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_root_not_map),
      cmocka_unit_test(test_class_key_not_symbol),
      cmocka_unit_test(test_class_value_not_map),
      cmocka_unit_test(test_object_type_not_int),
      cmocka_unit_test(test_alias_not_keyword),
      cmocka_unit_test(test_static_fields_not_map),
      cmocka_unit_test(test_instance_fns_not_map),
      cmocka_unit_test(test_static_field_key_not_symbol),
      cmocka_unit_test(test_intrinsic_key_not_symbol),
      cmocka_unit_test(test_intrinsic_value_not_vector),
      cmocka_unit_test(test_intrinsic_type_not_keyword),
      cmocka_unit_test(test_intrinsic_type_invalid),
      cmocka_unit_test(test_intrinsic_symbol_not_string),
      cmocka_unit_test(test_intrinsic_args_not_vector),
      cmocka_unit_test(test_unknown_arg_type),
      cmocka_unit_test(test_unknown_return_type),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
