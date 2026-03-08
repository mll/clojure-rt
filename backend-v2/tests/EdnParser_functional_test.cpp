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

static void run_test(void (*test_func)(void **), void **state,
                     const char *name) {
  try {
    test_func(state);
  } catch (const LanguageException &e) {
    llvm::symbolize::LLVMSymbolizer::Options options;
    llvm::symbolize::LLVMSymbolizer symbolizer(options);
    fprintf(stderr, "Test %s failed with LanguageException:\n%s\n", name,
            e.toString(symbolizer).c_str());
    assert_true(false);
  } catch (const std::exception &e) {
    fprintf(stderr, "Test %s failed with std::exception: %s\n", name, e.what());
    assert_true(false);
  } catch (...) {
    fprintf(stderr, "Test %s failed with unknown exception\n", name);
    assert_true(false);
  }
}

static void test_class_aliasing_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // Define Class A with alias :a
    // {com.foo/A {:alias :a, :object-type 100}}
    PersistentArrayMap *classAMap = PersistentArrayMap_empty();
    classAMap = PersistentArrayMap_assoc(classAMap,
                                         Keyword_create(String_create("alias")),
                                         Keyword_create(String_create("a")));
    classAMap = PersistentArrayMap_assoc(
        classAMap, Keyword_create(String_create("object-type")),
        RT_boxInt32(100));

    // Define Class B that uses alias :a
    // {com.foo/B {:instance-fns {msg [{:args [:a], :type :call, :symbol
    // "msg_sym"}]}}}
    PersistentArrayMap *msgMap = PersistentArrayMap_empty();
    PersistentVector *argsVec = PersistentVector_create();
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("a")));
    msgMap = PersistentArrayMap_assoc(
        msgMap, Keyword_create(String_create("args")), RT_boxPtr(argsVec));
    msgMap =
        PersistentArrayMap_assoc(msgMap, Keyword_create(String_create("type")),
                                 Keyword_create(String_create("call")));
    msgMap = PersistentArrayMap_assoc(msgMap,
                                      Keyword_create(String_create("symbol")),
                                      RT_boxPtr(String_create("msg_sym")));

    PersistentVector *msgOverloads = PersistentVector_create();
    msgOverloads = PersistentVector_conj(msgOverloads, RT_boxPtr(msgMap));

    PersistentArrayMap *instanceFnsMap = PersistentArrayMap_empty();
    instanceFnsMap = PersistentArrayMap_assoc(
        instanceFnsMap, Symbol_create(String_create("msg")),
        RT_boxPtr(msgOverloads));

    PersistentArrayMap *classBMap = PersistentArrayMap_empty();
    classBMap = PersistentArrayMap_assoc(
        classBMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(instanceFnsMap));

    // Root map: {com.foo/A classAMap, com.foo/B classBMap}
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/A")),
        RT_boxPtr(classAMap));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/B")),
        RT_boxPtr(classBMap));

    vector<ClassDescription> classes = buildClasses(RT_boxPtr(rootMap));

    assert_int_equal(2, classes.size());

    ClassDescription *descA = nullptr;
    ClassDescription *descB = nullptr;
    for (auto &c : classes) {
      if (c.name == "com.foo/A")
        descA = &c;
      if (c.name == "com.foo/B")
        descB = &c;
    }

    assert_non_null(descA);
    assert_non_null(descB);

    assert_true(descA->type.isDetermined());
    assert_int_equal(100, (int)descA->type.determinedType());

    // Check B's intrinsic args
    auto it = descB->instanceFns.find("msg");
    assert_true(it != descB->instanceFns.end());
    assert_int_equal(1, it->second.size());
    assert_int_equal(1, it->second[0].argTypes.size());
    assert_true(it->second[0].argTypes[0].isDetermined());
    assert_int_equal(100, (int)it->second[0].argTypes[0].determinedType());
  });
}

static void test_class_aliasing(void **state) {
  run_test(test_class_aliasing_impl, state, "test_class_aliasing");
}

static void test_static_fields_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // {com.foo/MyClass {:static-fields {VERSION "1.0", COUNT 42}}}
    PersistentArrayMap *fieldsMap = PersistentArrayMap_empty();
    fieldsMap = PersistentArrayMap_assoc(
        fieldsMap, Symbol_create(String_create("VERSION")),
        RT_boxPtr(String_create("1.0")));
    fieldsMap = PersistentArrayMap_assoc(
        fieldsMap, Symbol_create(String_create("COUNT")), RT_boxInt32(42));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxPtr(fieldsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/MyClass")),
        RT_boxPtr(classMap));

    vector<ClassDescription> classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(1, classes.size());

    auto &c = classes[0];
    assert_int_equal(2, c.staticFields.size());

    auto itVersion = c.staticFields.find("VERSION");
    assert_true(itVersion != c.staticFields.end());
    assert_string_equal("1.0",
                        String_c_str((String *)RT_unboxPtr(itVersion->second)));

    auto itCount = c.staticFields.find("COUNT");
    assert_true(itCount != c.staticFields.end());
    assert_int_equal(42, RT_unboxInt32(itCount->second));
  });
}

static void test_static_fields(void **state) {
  run_test(test_static_fields_impl, state, "test_static_fields");
}

static void test_special_types_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // {com.foo/A {:instance-fns {f [{:args [:this :any :nil], :returns :nil,
    // :type :call, :symbol "f_sym"}]}}}
    PersistentVector *argsVec = PersistentVector_create();
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("this")));
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("any")));
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("nil")));

    PersistentArrayMap *fMap = PersistentArrayMap_empty();
    fMap = PersistentArrayMap_assoc(fMap, Keyword_create(String_create("args")),
                                    RT_boxPtr(argsVec));
    fMap =
        PersistentArrayMap_assoc(fMap, Keyword_create(String_create("returns")),
                                 Keyword_create(String_create("nil")));
    fMap = PersistentArrayMap_assoc(fMap, Keyword_create(String_create("type")),
                                    Keyword_create(String_create("call")));
    fMap =
        PersistentArrayMap_assoc(fMap, Keyword_create(String_create("symbol")),
                                 RT_boxPtr(String_create("f_sym")));

    PersistentVector *fOverloads = PersistentVector_create();
    fOverloads = PersistentVector_conj(fOverloads, RT_boxPtr(fMap));

    PersistentArrayMap *instanceFnsMap = PersistentArrayMap_empty();
    instanceFnsMap = PersistentArrayMap_assoc(instanceFnsMap,
                                              Symbol_create(String_create("f")),
                                              RT_boxPtr(fOverloads));

    PersistentArrayMap *classAMap = PersistentArrayMap_empty();
    classAMap = PersistentArrayMap_assoc(
        classAMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(instanceFnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/A")),
        RT_boxPtr(classAMap));

    vector<ClassDescription> classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(1, classes.size());

    auto &c = classes[0];
    auto it = c.instanceFns.find("f");
    assert_true(it != c.instanceFns.end());
    auto &intrinsic = it->second[0];

    assert_int_equal(3, intrinsic.argTypes.size());
    assert_true(intrinsic.argTypes[0].contains(classType));
    assert_true(intrinsic.argTypes[1].isDynamic());
    assert_true(intrinsic.argTypes[2].contains(nilType));
    assert_true(intrinsic.returnType.contains(nilType));
  });
}

static void test_special_types(void **state) {
  run_test(test_special_types_impl, state, "test_special_types");
}

int main(void) {
  initialise_memory();
  // Warm up the static empty map to avoid false leak report in first test
  PersistentArrayMap_empty();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_class_aliasing),
      cmocka_unit_test(test_static_fields),
      cmocka_unit_test(test_special_types),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
