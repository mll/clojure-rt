#include "../../RuntimeHeaders.h"
#include "../../bytecode.pb.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include "../../runtime/Keyword.h"
#include "../../runtime/PersistentArrayMap.h"
#include "../../runtime/PersistentVector.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/String.h"
#include "../../runtime/Symbol.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;

static void test_trivial_memory(void **state) {
  (void)state; // unused

  ASSERT_MEMORY_ALL_BALANCED({
    RTValue val = RT_boxNil();
    release(val);
  });
}

static void test_edn_parser_memory(void **state) {
  (void)state; // unused

  // Ensure libprotobuf matches installed headers
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    llvm::orc::ExecutorAddr res =
        engine
            .compileAST(astClasses.nodes(0), "__classes",
                        llvm::OptimizationLevel::O0, false)
            .get();

    RTValue classes = res.toPtr<RTValue (*)()>()();

    // Release the final map holding the entire structure
    release(classes);
  });
}

static void test_edn_parser_class_parsing_memory(void **state) {
  (void)state; // unused

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    llvm::orc::ExecutorAddr res =
        engine
            .compileAST(astClasses.nodes(0), "__classes",
                        llvm::OptimizationLevel::O0, false)
            .get();

    RTValue classes = res.toPtr<RTValue (*)()>()();

    {
      try {
        vector<rt::ClassDescription> classesList = rt::buildClasses(classes);
        assert_true(classesList.size() > 0);
      } catch (const rt::LanguageException &e) {
        cout << rt::getExceptionString(e) << endl;
        assert_true(false);
      } catch (...) {
        printf("Unknown exception caught\n");
        assert_true(false);
      }
    }
  });
}

static void run_test(void (*test_func)(void **), void **state,
                     const char *name) {
  try {
    test_func(state);
  } catch (const LanguageException &e) {
    fprintf(stderr, "Test %s failed with LanguageException:\n%s\n", name,
            rt::getExceptionString(e).c_str());
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

int main(int argc, char **argv) {
  initialise_memory();
  PersistentArrayMap_empty();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_trivial_memory),
      cmocka_unit_test(test_edn_parser_memory),
      cmocka_unit_test(test_edn_parser_class_parsing_memory),
      cmocka_unit_test(test_class_aliasing),
      cmocka_unit_test(test_static_fields),
      cmocka_unit_test(test_special_types),
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
