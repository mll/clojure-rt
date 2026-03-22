#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
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

#include <fstream>

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
            .get().address;

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
            .get().address;

    RTValue classes = res.toPtr<RTValue (*)()>()();

    {
      try {
        auto classesList = rt::buildClasses(classes);
        assert_true(classesList.size() > 0);

        // Verification of specific classes from rt-classes.edn
        rt::ClassDescription *objDesc = nullptr;
        rt::ClassDescription *longDesc = nullptr;
        rt::ClassDescription *numbersDesc = nullptr;
        rt::ClassDescription *stringDesc = nullptr;

        for (auto &c : classesList) {
          if (c->name == "java.lang.Object")
            objDesc = c.get();
          else if (c->name == "java.lang.Long")
            longDesc = c.get();
          else if (c->name == "clojure.lang.Numbers")
            numbersDesc = c.get();
          else if (c->name == "java.lang.String")
            stringDesc = c.get();
          else if (c->name == "java.lang.Class") {
            assert_string_equal("java.lang.Object", c->parentName.c_str());
          }
        }

        // Verify java.lang.Object
        assert_non_null(objDesc);
        assert_true(objDesc->type.isDynamic()); // :any becomes dynamic type

        // Verify java.lang.Long
        assert_non_null(longDesc);
        assert_int_equal(1, (int)longDesc->type.determinedType());
        assert_true(longDesc->type.contains(integerType));
        // Verify ALL static fields in java.lang.Long
        assert_int_equal(4, longDesc->staticFields.size());
        assert_int_equal(4, RT_unboxInt32(longDesc->staticFields.at("BYTES")));
        assert_int_equal(2147483647,
                         RT_unboxInt32(longDesc->staticFields.at("MAX_VALUE")));
        assert_int_equal(-2147483647,
                         RT_unboxInt32(longDesc->staticFields.at("MIN_VALUE")));
        assert_int_equal(32, RT_unboxInt32(longDesc->staticFields.at("SIZE")));

        // Verify clojure.lang.Numbers
        assert_non_null(numbersDesc);
        auto addIt = numbersDesc->staticFns.find("add");
        assert_true(addIt != numbersDesc->staticFns.end());
        auto &addOverloads = addIt->second;
        assert_int_equal(17, addOverloads.size());

        // Overload 0: [:double :double] -> :double (intrinsic)
        assert_int_equal(2, addOverloads[0].argTypes.size());
        assert_true(addOverloads[0].argTypes[0].contains(doubleType));
        assert_true(addOverloads[0].argTypes[1].contains(doubleType));
        assert_true(addOverloads[0].returnType.contains(doubleType));
        assert_int_equal((int)rt::CallType::Intrinsic,
                         (int)addOverloads[0].type);
        assert_string_equal("FAdd", addOverloads[0].symbol.c_str());

        // Overload 1: [:int :int] -> :int (intrinsic)
        assert_int_equal(2, addOverloads[1].argTypes.size());
        assert_true(addOverloads[1].argTypes[0].contains(integerType));
        assert_true(addOverloads[1].argTypes[1].contains(integerType));
        assert_true(addOverloads[1].returnType.contains(integerType));
        assert_int_equal((int)rt::CallType::Intrinsic,
                         (int)addOverloads[1].type);

        // Overload 2: [:any :any] -> :any (call)
        assert_int_equal(2, addOverloads[16].argTypes.size());
        assert_true(addOverloads[16].argTypes[0].isDynamic());
        assert_true(addOverloads[16].argTypes[1].isDynamic());
        assert_true(addOverloads[16].returnType.isDynamic());
        assert_int_equal((int)rt::CallType::Call, (int)addOverloads[16].type);

        // Verify clojure.lang.Numbers/gte (dynamic alias :bool)
        auto gteIt = numbersDesc->staticFns.find("gte");
        assert_true(gteIt != numbersDesc->staticFns.end());
        assert_int_equal(15, gteIt->second.size());
        // Verify return type of first overload is booleanType (via :bool alias)
        assert_true(gteIt->second[0].returnType.contains(booleanType));

        // Verify java.lang.String
        assert_non_null(stringDesc);
        assert_int_equal(7, (int)stringDesc->type.determinedType());
        // Verify instance fns
        auto containsIt = stringDesc->instanceFns.find("contains");
        assert_true(containsIt != stringDesc->instanceFns.end());
        assert_int_equal(1, containsIt->second.size());
        assert_string_equal("String_contains",
                            containsIt->second[0].symbol.c_str());
        assert_true(containsIt->second[0].returnType.contains(booleanType));

        auto replaceIt = stringDesc->instanceFns.find("replace");
        assert_true(replaceIt != stringDesc->instanceFns.end());
        assert_int_equal(1, replaceIt->second.size());
        // Verify full symbol "java.lang.String" resolution
        assert_true(replaceIt->second[0].argTypes[0].contains(stringType));
        assert_true(replaceIt->second[0].returnType.contains(stringType));

        auto indexOfIt = stringDesc->instanceFns.find("indexOf");
        assert_true(indexOfIt != stringDesc->instanceFns.end());
        assert_int_equal(2, indexOfIt->second.size()); // Two overloads

        // indexOf(String)
        assert_int_equal(2, indexOfIt->second[0].argTypes.size());
        assert_true(indexOfIt->second[0].argTypes[0].contains(stringType));

        // indexOf(String, int)
        assert_int_equal(3, indexOfIt->second[1].argTypes.size());
        assert_true(indexOfIt->second[1].argTypes[0].contains(stringType));
        assert_true(indexOfIt->second[1].argTypes[1].contains(stringType));
        assert_true(indexOfIt->second[1].argTypes[2].contains(integerType));

        // Verify java.lang.Object/toString returns java.lang.String (full
        // symbol)
        assert_non_null(objDesc);
        auto toStringIt = objDesc->instanceFns.find("toString");
        assert_true(toStringIt != objDesc->instanceFns.end());
        assert_true(toStringIt->second[0].returnType.contains(stringType));

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

static void execute_test(void (*test_func)(void **), void **state,
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
        PersistentVector_conj(argsVec, Keyword_create(String_create("this")));
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

    // After assoc, rootMap owns a reference, but we must release our local
    // ones.

    auto classes = buildClasses(RT_boxPtr(rootMap));

    assert_int_equal(2, classes.size());

    ClassDescription *descA = nullptr;
    ClassDescription *descB = nullptr;
    for (auto &c : classes) {
      if (c->name == "com.foo/A")
        descA = c.get();
      if (c->name == "com.foo/B")
        descB = c.get();
    }

    assert_non_null(descA);
    assert_non_null(descB);

    assert_true(descA->type.isDetermined());
    assert_int_equal(100, (int)descA->type.determinedType());

    // Check B's intrinsic args
    auto it = descB->instanceFns.find("msg");
    assert_true(it != descB->instanceFns.end());
    assert_int_equal(1, it->second.size());
    assert_int_equal(2, it->second[0].argTypes.size());
    assert_true(it->second[0].argTypes[1].isDetermined());
    assert_int_equal(100, (int)it->second[0].argTypes[1].determinedType());
    assert_true(it->second[0].argTypes[0].isDetermined());
    assert_int_equal(15, (int)it->second[0].argTypes[0].determinedType());
  });
}

static void test_class_aliasing(void **state) {
  execute_test(test_class_aliasing_impl, state, "test_class_aliasing");
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

    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(1, classes.size());

    auto &c = *classes[0];
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
  execute_test(test_static_fields_impl, state, "test_static_fields");
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

    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(1, classes.size());

    auto &c = *classes[0];
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

static void test_bridge_functions_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // {com.foo/Test {:instance-fields ["a" "b"], :static-fields {S1 10, S2
    // 20}}}
    PersistentVector *ifields = PersistentVector_create();
    ifields = PersistentVector_conj(ifields, RT_boxPtr(String_create("a")));
    ifields = PersistentVector_conj(ifields, RT_boxPtr(String_create("b")));

    PersistentArrayMap *sfields = PersistentArrayMap_empty();
    sfields = PersistentArrayMap_assoc(
        sfields, Symbol_create(String_create("S1")), RT_boxInt32(10));
    sfields = PersistentArrayMap_assoc(
        sfields, Symbol_create(String_create("S2")), RT_boxInt32(20));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fields")),
        RT_boxPtr(ifields));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxPtr(sfields));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/Test")),
        RT_boxPtr(classMap));

    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(1, classes.size());

    ClassDescription &desc = *classes[0];
    void *ext = &desc;

    // Test instance field indices
    RTValue fieldA = Symbol_create(String_create("a"));
    RTValue fieldB = Symbol_create(String_create("b"));
    RTValue fieldC = Symbol_create(String_create("c"));

    assert_int_equal(0, ClassExtension_fieldIndex(ext, fieldA));
    assert_int_equal(1, ClassExtension_fieldIndex(ext, fieldB));
    assert_int_equal(-1, ClassExtension_fieldIndex(ext, fieldC));

    release(fieldA);
    release(fieldB);
    release(fieldC);

    // Test static field indices
    RTValue sfield1 = Symbol_create(String_create("S1"));
    RTValue sfield2 = Symbol_create(String_create("S2"));

    int32_t idx1 = ClassExtension_staticFieldIndex(ext, sfield1);
    int32_t idx2 = ClassExtension_staticFieldIndex(ext, sfield2);

    assert_true(idx1 != -1);
    assert_true(idx2 != -1);
    assert_true(idx1 != idx2);

    // Test static field values
    RTValue val1 = ClassExtension_getIndexedStaticField(ext, idx1);
    RTValue val2 = ClassExtension_getIndexedStaticField(ext, idx2);

    // Values are retained by getIndexedStaticField
    assert_int_equal(10, RT_unboxInt32(val1));
    assert_int_equal(20, RT_unboxInt32(val2));

    release(val1);
    release(val2);

    // Test setting static field
    RTValue newVal = RT_boxInt32(100);
    ClassExtension_setIndexedStaticField(ext, idx1, newVal);
    RTValue checkVal = ClassExtension_getIndexedStaticField(ext, idx1);
    assert_int_equal(100, RT_unboxInt32(checkVal));
    release(checkVal);

    release(sfield1);
    release(sfield2);
  });
}

static void test_bridge_functions(void **state) {
  execute_test(test_bridge_functions_impl, state, "test_bridge_functions");
}

static void test_special_types(void **state) {
  execute_test(test_special_types_impl, state, "test_special_types");
}

static void execute_negative_test(void (*test_func)(void **), void **state,
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
  execute_negative_test(test_root_not_map_impl, state, "test_root_not_map");
}

static void test_class_key_not_symbol_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(rootMap, RT_boxInt32(1),
                                       RT_boxPtr(PersistentArrayMap_empty()));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_class_key_not_symbol(void **state) {
  execute_negative_test(test_class_key_not_symbol_impl, state,
                        "test_class_key_not_symbol");
}

static void test_class_value_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxInt32(1));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_class_value_not_map(void **state) {
  execute_negative_test(test_class_value_not_map_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_object_type_not_int(void **state) {
  execute_negative_test(test_object_type_not_int_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_alias_not_keyword(void **state) {
  execute_negative_test(test_alias_not_keyword_impl, state,
                        "test_alias_not_keyword");
}

// --- Group 2: ClassDescription ---

static void test_static_fields_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxInt32(1));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_static_fields_not_map(void **state) {
  execute_negative_test(test_static_fields_not_map_impl, state,
                        "test_static_fields_not_map");
}

static void test_instance_fns_not_map_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxInt32(1));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_instance_fns_not_map(void **state) {
  execute_negative_test(test_instance_fns_not_map_impl, state,
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
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxPtr(fieldsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_static_field_key_not_symbol(void **state) {
  execute_negative_test(test_static_field_key_not_symbol_impl, state,
                        "test_static_field_key_not_symbol");
}

static void test_intrinsic_key_not_symbol_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, RT_boxInt32(1),
                                      RT_boxPtr(PersistentVector_create()));
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_key_not_symbol(void **state) {
  execute_negative_test(test_intrinsic_key_not_symbol_impl, state,
                        "test_intrinsic_key_not_symbol");
}

static void test_intrinsic_value_not_vector_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("f")),
                                      RT_boxInt32(42));
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(fnsMap));
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("A")), RT_boxPtr(classMap));
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_value_not_vector(void **state) {
  execute_negative_test(test_intrinsic_value_not_vector_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_type_not_keyword(void **state) {
  execute_negative_test(test_intrinsic_type_not_keyword_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_type_invalid(void **state) {
  execute_negative_test(test_intrinsic_type_invalid_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_symbol_not_string(void **state) {
  execute_negative_test(test_intrinsic_symbol_not_string_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_intrinsic_args_not_vector(void **state) {
  execute_negative_test(test_intrinsic_args_not_vector_impl, state,
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_unknown_arg_type(void **state) {
  execute_negative_test(test_unknown_arg_type_impl, state,
                        "test_unknown_arg_type");
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
    auto classes = buildClasses(RT_boxPtr(rootMap));
  });
}

static void test_unknown_return_type(void **state) {
  execute_negative_test(test_unknown_return_type_impl, state,
                        "test_unknown_return_type");
}

static void test_class_inheritance_linkage_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;

    // {Parent {:object-type 100}, Child {:extends Parent, :object-type 101}}
    PersistentArrayMap *parentMap = PersistentArrayMap_empty();
    parentMap = PersistentArrayMap_assoc(
        parentMap, Keyword_create(String_create("object-type")),
        RT_boxInt32(100));

    PersistentArrayMap *childMap = PersistentArrayMap_empty();
    childMap = PersistentArrayMap_assoc(
        childMap, Keyword_create(String_create("extends")),
        Symbol_create(String_create("Parent")));
    childMap = PersistentArrayMap_assoc(
        childMap, Keyword_create(String_create("object-type")),
        RT_boxInt32(101));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("Parent")), RT_boxPtr(parentMap));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("Child")), RT_boxPtr(childMap));

    compState.storeInternalClasses(RT_boxPtr(rootMap));

    ::Class *parent = compState.classRegistry.getCurrent("Parent");
    ::Class *child = compState.classRegistry.getCurrent("Child");

    assert_non_null(parent);
    assert_non_null(child);

    // Check linkage (only in ClassDescription)
    assert_int_equal(0, child->superclassCount);
    assert_null(child->superclasses);

    rt::ClassDescription *childDesc =
        static_cast<rt::ClassDescription *>(child->compilerExtension);
    assert_ptr_equal(parent, childDesc->extends);

    Ptr_release(parent);
    Ptr_release(child);
  });
}

static void test_toString_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // {com.foo/MyClass {:object-type 100, :extends "Parent",
    //                  :instance-fields ["a" "b"],
    //                  :static-fields {:S1 10, :S2 20},
    //                  :static-fns {s_func [{:args [:int], :type :intrinsic,
    //                                        :symbol "s1_sym"}
    //                                       {:args [:any :any], :type :call,
    //                                        :symbol "s2_sym"}],
    //                               s_func2 [{:args [:nil], :type :call,
    //                                         :symbol "s3_sym"}]},
    //                  :instance-fns {f [{:args [:this :any], :type :call,
    //                                     :symbol "f_sym"}],
    //                                 g [{:args [:this], :type :intrinsic,
    //                                     :symbol "g_sym"}]}}}
    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("object-type")),
        RT_boxInt32(100));
    classMap = PersistentArrayMap_assoc(classMap,
                                         Keyword_create(String_create("extends")),
                                         RT_boxPtr(String_create("Parent")));

    PersistentVector *ifields = PersistentVector_create();
    ifields = PersistentVector_conj(ifields, RT_boxPtr(String_create("a")));
    ifields = PersistentVector_conj(ifields, RT_boxPtr(String_create("b")));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fields")),
        RT_boxPtr(ifields));

    PersistentArrayMap *sfields = PersistentArrayMap_empty();
    sfields = PersistentArrayMap_assoc(
        sfields, Symbol_create(String_create("S1")), RT_boxInt32(10));
    sfields = PersistentArrayMap_assoc(
        sfields, Symbol_create(String_create("S2")), RT_boxInt32(20));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fields")),
        RT_boxPtr(sfields));

    PersistentVector *argsVec = PersistentVector_create();
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("this")));
    argsVec =
        PersistentVector_conj(argsVec, Keyword_create(String_create("any")));

    // Instance function 1: f [:this :any] -> :call "f_sym"
    PersistentArrayMap *fMap = PersistentArrayMap_empty();
    fMap = PersistentArrayMap_assoc(fMap, Keyword_create(String_create("args")),
                                    RT_boxPtr(argsVec));
    fMap =
        PersistentArrayMap_assoc(fMap, Keyword_create(String_create("type")),
                                 Keyword_create(String_create("call")));
    fMap =
        PersistentArrayMap_assoc(fMap, Keyword_create(String_create("symbol")),
                                 RT_boxPtr(String_create("f_sym")));
    PersistentVector *fOverloads = PersistentVector_create();
    fOverloads = PersistentVector_conj(fOverloads, RT_boxPtr(fMap));

    // Instance function 2: g [:this] -> :intrinsic "g_sym"
    PersistentVector *gArgs = PersistentVector_create();
    gArgs = PersistentVector_conj(gArgs, Keyword_create(String_create("this")));
    PersistentArrayMap *gMap = PersistentArrayMap_empty();
    gMap = PersistentArrayMap_assoc(gMap, Keyword_create(String_create("args")), RT_boxPtr(gArgs));
    gMap = PersistentArrayMap_assoc(gMap, Keyword_create(String_create("type")), Keyword_create(String_create("intrinsic")));
    gMap = PersistentArrayMap_assoc(gMap, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("g_sym")));
    PersistentVector *gOverloads = PersistentVector_create();
    gOverloads = PersistentVector_conj(gOverloads, RT_boxPtr(gMap));

    PersistentArrayMap *instanceFnsMap = PersistentArrayMap_empty();
    instanceFnsMap = PersistentArrayMap_assoc(instanceFnsMap,
                                              Symbol_create(String_create("f")),
                                              RT_boxPtr(fOverloads));
    instanceFnsMap = PersistentArrayMap_assoc(instanceFnsMap,
                                              Symbol_create(String_create("g")),
                                              RT_boxPtr(gOverloads));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("instance-fns")),
        RT_boxPtr(instanceFnsMap));

    // Static function s_func: arity 1 and arity 2
    PersistentVector *s1Args = PersistentVector_create();
    s1Args = PersistentVector_conj(s1Args, Keyword_create(String_create("int")));
    PersistentArrayMap *s1Map = PersistentArrayMap_empty();
    s1Map = PersistentArrayMap_assoc(s1Map, Keyword_create(String_create("args")), RT_boxPtr(s1Args));
    s1Map = PersistentArrayMap_assoc(s1Map, Keyword_create(String_create("type")), Keyword_create(String_create("intrinsic")));
    s1Map = PersistentArrayMap_assoc(s1Map, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("s1_sym")));

    PersistentVector *s2Args = PersistentVector_create();
    s2Args = PersistentVector_conj(s2Args, Keyword_create(String_create("any")));
    s2Args = PersistentVector_conj(s2Args, Keyword_create(String_create("any")));
    PersistentArrayMap *s2Map = PersistentArrayMap_empty();
    s2Map = PersistentArrayMap_assoc(s2Map, Keyword_create(String_create("args")), RT_boxPtr(s2Args));
    s2Map = PersistentArrayMap_assoc(s2Map, Keyword_create(String_create("type")), Keyword_create(String_create("call")));
    s2Map = PersistentArrayMap_assoc(s2Map, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("s2_sym")));

    PersistentVector *sOverloads = PersistentVector_create();
    sOverloads = PersistentVector_conj(sOverloads, RT_boxPtr(s1Map));
    sOverloads = PersistentVector_conj(sOverloads, RT_boxPtr(s2Map));

    // Static function s_func2: arity 1
    PersistentVector *s3Args = PersistentVector_create();
    s3Args = PersistentVector_conj(s3Args, Keyword_create(String_create("nil")));
    PersistentArrayMap *s3Map = PersistentArrayMap_empty();
    s3Map = PersistentArrayMap_assoc(s3Map, Keyword_create(String_create("args")), RT_boxPtr(s3Args));
    s3Map = PersistentArrayMap_assoc(s3Map, Keyword_create(String_create("type")), Keyword_create(String_create("call")));
    s3Map = PersistentArrayMap_assoc(s3Map, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("s3_sym")));
    PersistentVector *s3Overloads = PersistentVector_create();
    s3Overloads = PersistentVector_conj(s3Overloads, RT_boxPtr(s3Map));

    PersistentArrayMap *staticFnsMap = PersistentArrayMap_empty();
    staticFnsMap = PersistentArrayMap_assoc(
        staticFnsMap, Symbol_create(String_create("s_func")),
        RT_boxPtr(sOverloads));
    staticFnsMap = PersistentArrayMap_assoc(
        staticFnsMap, Symbol_create(String_create("s_func2")),
        RT_boxPtr(s3Overloads));
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fns")),
        RT_boxPtr(staticFnsMap));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/MyClass")),
        RT_boxPtr(classMap));

    PersistentArrayMap *intMap = PersistentArrayMap_empty();
    intMap = PersistentArrayMap_assoc(
        intMap, Keyword_create(String_create("alias")),
        Keyword_create(String_create("int")));
    intMap = PersistentArrayMap_assoc(
        intMap, Keyword_create(String_create("object-type")), RT_boxInt32(1));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("int")), RT_boxPtr(intMap));

    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(2, classes.size());

    ClassDescription *myClass = nullptr;
    for (auto &c : classes) {
      if (c->name == "com.foo/MyClass")
        myClass = c.get();
    }
    assert_non_null(myClass);
    string s = myClass->toString();

    assert_string_contains(s, "{com.foo/MyClass");
    assert_string_contains(s, ":extends Parent");
    assert_string_contains(s, ":static-fields {");
    assert_string_contains(s, ":S1 10");
    assert_string_contains(s, ":S2 20");
    assert_string_contains(s, ":instance-fields [\"a\" \"b\"]");
    assert_string_contains(s, ":instance-fns {");
    assert_string_contains(s, "f [{:symbol \"f_sym\"");
    assert_string_contains(s, "g [{:symbol \"g_sym\"");
    assert_string_contains(s, ":static-fns {");
    assert_string_contains(s, "s_func [{:symbol \"s1_sym\"");
    assert_string_contains(s, "{:symbol \"s2_sym\"");
    assert_string_contains(s, "s_func2 [{:symbol \"s3_sym\"");
    assert_string_contains(s, ":args [:this :any]");
    assert_string_contains(s, ":args [:this]");
  });
}

static void test_toString(void **state) {
  execute_test(test_toString_impl, state, "test_toString");
}

static void test_class_inheritance_linkage(void **state) {
  execute_test(test_class_inheritance_linkage_impl, state,
               "test_class_inheritance_linkage");
}

static void test_overload_symbols_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // Define a class with overloads having different symbols
    // {Math
    //  {:static-fns
    //   {sin   [{:symbol "Math_sin_D", :args [:double] :type :intrinsic}
    //           {:symbol "Math_sin_I", :args [:int]    :type :intrinsic}
    //           {:symbol "Math_sin_B", :args [:bigint] :type :intrinsic}]
    //    cos   [{:symbol "Math_cos_D", :args [:double] :type :intrinsic}
    //           {:symbol "Math_cos_I", :args [:int]    :type :intrinsic}
    //           {:symbol "Math_cos_B", :args [:bigint] :type :intrinsic}]
    //    tan   [{:symbol "Math_tan_D", :args [:double] :type :intrinsic}]
    //    atan2 [{:symbol "Math_atan2_DD", :args [:double :double] :type
    //    :intrinsic}]
    //    pow   [{:symbol "Math_pow_DD", :args [:double :double] :type
    //    :intrinsic}
    //           {:symbol "Math_pow_II", :args [:int :int]       :type
    //           :intrinsic}]
    //    abs   [{:symbol "Math_abs_D", :args [:double] :type :intrinsic}
    //           {:symbol "Math_abs_I", :args [:int]    :type :intrinsic}
    //           {:symbol "Math_abs_B", :args [:bigint] :type :intrinsic}]
    //    log   [{:symbol "Math_log_D", :args [:double] :type :intrinsic}]}}}

    PersistentArrayMap *sin1 = PersistentArrayMap_empty();
    sin1 =
        PersistentArrayMap_assoc(sin1, Keyword_create(String_create("symbol")),
                                 RT_boxPtr(String_create("Math_sin_D")));
    sin1 = PersistentArrayMap_assoc(sin1, Keyword_create(String_create("type")),
                                    Keyword_create(String_create("intrinsic")));
    PersistentVector *args1 = PersistentVector_create();
    args1 =
        PersistentVector_conj(args1, Keyword_create(String_create("double")));
    sin1 = PersistentArrayMap_assoc(sin1, Keyword_create(String_create("args")),
                                    RT_boxPtr(args1));

    PersistentArrayMap *sin2 = PersistentArrayMap_empty();
    sin2 =
        PersistentArrayMap_assoc(sin2, Keyword_create(String_create("symbol")),
                                 RT_boxPtr(String_create("Math_sin_I")));
    sin2 = PersistentArrayMap_assoc(sin2, Keyword_create(String_create("type")),
                                    Keyword_create(String_create("intrinsic")));
    PersistentVector *args2 = PersistentVector_create();
    args2 = PersistentVector_conj(args2, Keyword_create(String_create("int")));
    sin2 = PersistentArrayMap_assoc(sin2, Keyword_create(String_create("args")),
                                    RT_boxPtr(args2));

    PersistentArrayMap *sin3 = PersistentArrayMap_empty();
    sin3 =
        PersistentArrayMap_assoc(sin3, Keyword_create(String_create("symbol")),
                                 RT_boxPtr(String_create("Math_sin_B")));
    sin3 = PersistentArrayMap_assoc(sin3, Keyword_create(String_create("type")),
                                    Keyword_create(String_create("intrinsic")));
    PersistentVector *args3 = PersistentVector_create();
    args3 =
        PersistentVector_conj(args3, Keyword_create(String_create("bigint")));
    sin3 = PersistentArrayMap_assoc(sin3, Keyword_create(String_create("args")),
                                    RT_boxPtr(args3));

    PersistentVector *sinOverloads = PersistentVector_create();
    sinOverloads = PersistentVector_conj(sinOverloads, RT_boxPtr(sin1));
    sinOverloads = PersistentVector_conj(sinOverloads, RT_boxPtr(sin2));
    sinOverloads = PersistentVector_conj(sinOverloads, RT_boxPtr(sin3));

    auto createSimpleIntrinsic = [](const char *sym, const char *argType) {
      PersistentArrayMap *m = PersistentArrayMap_empty();
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("symbol")),
                                   RT_boxPtr(String_create(sym)));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("type")),
                                   Keyword_create(String_create("intrinsic")));
      PersistentVector *args = PersistentVector_create();
      args = PersistentVector_conj(args, Keyword_create(String_create(argType)));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("args")),
                                   RT_boxPtr(args));
      return RT_boxPtr(m);
    };

    auto createBinaryIntrinsic = [](const char *sym, const char *arg1,
                                    const char *arg2) {
      PersistentArrayMap *m = PersistentArrayMap_empty();
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("symbol")),
                                   RT_boxPtr(String_create(sym)));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("type")),
                                   Keyword_create(String_create("intrinsic")));
      PersistentVector *args = PersistentVector_create();
      args = PersistentVector_conj(args, Keyword_create(String_create(arg1)));
      args = PersistentVector_conj(args, Keyword_create(String_create(arg2)));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("args")),
                                   RT_boxPtr(args));
      return RT_boxPtr(m);
    };

    PersistentVector *cosOverloads = PersistentVector_create();
    cosOverloads = PersistentVector_conj(
        cosOverloads, createSimpleIntrinsic("Math_cos_D", "double"));
    cosOverloads = PersistentVector_conj(
        cosOverloads, createSimpleIntrinsic("Math_cos_I", "int"));
    cosOverloads = PersistentVector_conj(
        cosOverloads, createSimpleIntrinsic("Math_cos_B", "bigint"));

    PersistentVector *tanOverloads = PersistentVector_create();
    tanOverloads = PersistentVector_conj(
        tanOverloads, createSimpleIntrinsic("Math_tan_D", "double"));

    PersistentVector *atan2Overloads = PersistentVector_create();
    atan2Overloads = PersistentVector_conj(
        atan2Overloads,
        createBinaryIntrinsic("Math_atan2_DD", "double", "double"));

    PersistentVector *powOverloads = PersistentVector_create();
    powOverloads = PersistentVector_conj(
        powOverloads, createBinaryIntrinsic("Math_pow_DD", "double", "double"));
    powOverloads = PersistentVector_conj(
        powOverloads, createBinaryIntrinsic("Math_pow_II", "int", "int"));

    PersistentVector *absOverloads = PersistentVector_create();
    absOverloads = PersistentVector_conj(
        absOverloads, createSimpleIntrinsic("Math_abs_D", "double"));
    absOverloads = PersistentVector_conj(
        absOverloads, createSimpleIntrinsic("Math_abs_I", "int"));
    absOverloads = PersistentVector_conj(
        absOverloads, createSimpleIntrinsic("Math_abs_B", "bigint"));

    PersistentVector *logOverloads = PersistentVector_create();
    logOverloads = PersistentVector_conj(
        logOverloads, createSimpleIntrinsic("Math_log_D", "double"));

    PersistentArrayMap *staticFns = PersistentArrayMap_empty();
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("sin")),
                                 RT_boxPtr(sinOverloads));
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("cos")),
                                 RT_boxPtr(cosOverloads));
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("tan")),
                                 RT_boxPtr(tanOverloads));
    staticFns = PersistentArrayMap_assoc(
        staticFns, Symbol_create(String_create("atan2")),
        RT_boxPtr(atan2Overloads));
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("pow")),
                                 RT_boxPtr(powOverloads));
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("abs")),
                                 RT_boxPtr(absOverloads));
    staticFns =
        PersistentArrayMap_assoc(staticFns, Symbol_create(String_create("log")),
                                 RT_boxPtr(logOverloads));

    PersistentArrayMap *classMap = PersistentArrayMap_empty();
    classMap = PersistentArrayMap_assoc(
        classMap, Keyword_create(String_create("static-fns")),
        RT_boxPtr(staticFns));

    PersistentArrayMap *intMap = PersistentArrayMap_empty();
    intMap = PersistentArrayMap_assoc(
        intMap, Keyword_create(String_create("alias")),
        Keyword_create(String_create("int")));
    intMap = PersistentArrayMap_assoc(
        intMap, Keyword_create(String_create("object-type")), RT_boxInt32(1));

    PersistentArrayMap *doubleMap = PersistentArrayMap_empty();
    doubleMap = PersistentArrayMap_assoc(
        doubleMap, Keyword_create(String_create("alias")),
        Keyword_create(String_create("double")));
    doubleMap = PersistentArrayMap_assoc(
        doubleMap, Keyword_create(String_create("object-type")), RT_boxInt32(5));

    PersistentArrayMap *bigintMap = PersistentArrayMap_empty();
    bigintMap = PersistentArrayMap_assoc(
        bigintMap, Keyword_create(String_create("alias")),
        Keyword_create(String_create("bigint")));
    bigintMap = PersistentArrayMap_assoc(
        bigintMap, Keyword_create(String_create("object-type")),
        RT_boxInt32(13));

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("Math")), RT_boxPtr(classMap));
    rootMap =
        PersistentArrayMap_assoc(rootMap, Symbol_create(String_create("int")),
                                 RT_boxPtr(intMap));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("double")), RT_boxPtr(doubleMap));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("bigint")), RT_boxPtr(bigintMap));

    // buildClasses will consume the root map.
    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(4, classes.size());

    ClassDescription *c = nullptr;
    for (auto &cls : classes) {
      if (cls->name == "Math")
        c = cls.get();
    }
    assert_non_null(c);

    auto assert_arg = [](const IntrinsicDescription &d, size_t idx,
                         objectType type) {
      assert_true(idx < d.argTypes.size());
      assert_int_equal(1, d.argTypes[idx].size());
      assert_true(d.argTypes[idx].contains(type));
      assert_false(d.argTypes[idx].isBoxedType());
    };

    auto it = c->staticFns.find("sin");
    assert_true(it != c->staticFns.end());
    assert_int_equal(3, it->second.size());
    assert_string_equal("Math_sin_D", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5); // double
    assert_string_equal("Math_sin_I", it->second[1].symbol.c_str());
    assert_arg(it->second[1], 0, (objectType)1); // int
    assert_string_equal("Math_sin_B", it->second[2].symbol.c_str());
    assert_arg(it->second[2], 0, (objectType)13); // bigint

    it = c->staticFns.find("cos");
    assert_true(it != c->staticFns.end());
    assert_int_equal(3, it->second.size());
    assert_string_equal("Math_cos_D", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);
    assert_string_equal("Math_cos_I", it->second[1].symbol.c_str());
    assert_arg(it->second[1], 0, (objectType)1);
    assert_string_equal("Math_cos_B", it->second[2].symbol.c_str());
    assert_arg(it->second[2], 0, (objectType)13);

    it = c->staticFns.find("tan");
    assert_true(it != c->staticFns.end());
    assert_int_equal(1, it->second.size());
    assert_string_equal("Math_tan_D", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);

    it = c->staticFns.find("atan2");
    assert_true(it != c->staticFns.end());
    assert_int_equal(1, it->second.size());
    assert_string_equal("Math_atan2_DD", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);
    assert_arg(it->second[0], 1, (objectType)5);

    it = c->staticFns.find("pow");
    assert_true(it != c->staticFns.end());
    assert_int_equal(2, it->second.size());
    assert_string_equal("Math_pow_DD", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);
    assert_arg(it->second[0], 1, (objectType)5);
    assert_string_equal("Math_pow_II", it->second[1].symbol.c_str());
    assert_arg(it->second[1], 0, (objectType)1);
    assert_arg(it->second[1], 1, (objectType)1);

    it = c->staticFns.find("abs");
    assert_true(it != c->staticFns.end());
    assert_int_equal(3, it->second.size());
    assert_string_equal("Math_abs_D", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);
    assert_string_equal("Math_abs_I", it->second[1].symbol.c_str());
    assert_arg(it->second[1], 0, (objectType)1);
    assert_string_equal("Math_abs_B", it->second[2].symbol.c_str());
    assert_arg(it->second[2], 0, (objectType)13);

    it = c->staticFns.find("log");
    assert_true(it != c->staticFns.end());
    assert_int_equal(1, it->second.size());
    assert_string_equal("Math_log_D", it->second[0].symbol.c_str());
    assert_arg(it->second[0], 0, (objectType)5);
  });
}

static void test_overload_symbols(void **state) {
  execute_test(test_overload_symbols_impl, state, "test_overload_symbols");
}

static void test_cross_class_references_impl(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    // {com.foo/A {:alias :a, :object-type 100, :static-fns {func [{:args [:b], :type :intrinsic, :symbol "A_func"}]}}
    //  com.foo/B {:alias :b, :object-type 101, :static-fns {func [{:args [:a], :type :intrinsic, :symbol "B_func"}]}}}

    auto createClass = [](const char *alias, const char *otherAlias,
                          const char *funcSym, int id) {
      PersistentArrayMap *intrinsic = PersistentArrayMap_empty();
      intrinsic = PersistentArrayMap_assoc(
          intrinsic, Keyword_create(String_create("symbol")),
          RT_boxPtr(String_create(funcSym)));
      intrinsic = PersistentArrayMap_assoc(
          intrinsic, Keyword_create(String_create("type")),
          Keyword_create(String_create("intrinsic")));
      PersistentVector *args = PersistentVector_create();
      args =
          PersistentVector_conj(args, Keyword_create(String_create(otherAlias)));
      intrinsic = PersistentArrayMap_assoc(
          intrinsic, Keyword_create(String_create("args")), RT_boxPtr(args));

      PersistentVector *overloads = PersistentVector_create();
      overloads = PersistentVector_conj(overloads, RT_boxPtr(intrinsic));

      PersistentArrayMap *staticFns = PersistentArrayMap_empty();
      staticFns = PersistentArrayMap_assoc(
          staticFns, Symbol_create(String_create("func")), RT_boxPtr(overloads));

      PersistentArrayMap *m = PersistentArrayMap_empty();
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("alias")),
                                   Keyword_create(String_create(alias)));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("static-fns")),
                                   RT_boxPtr(staticFns));
      m = PersistentArrayMap_assoc(m, Keyword_create(String_create("object-type")),
                                   RT_boxInt32(id));
      return RT_boxPtr(m);
    };

    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/A")),
        createClass("a", "b", "A_func", 100));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/B")),
        createClass("b", "a", "B_func", 101));

    auto classes = buildClasses(RT_boxPtr(rootMap));
    assert_int_equal(2, classes.size());

    ClassDescription *a = nullptr;
    ClassDescription *b = nullptr;
    for (auto &c : classes) {
      if (c->name == "com.foo/A")
        a = c.get();
      if (c->name == "com.foo/B")
        b = c.get();
    }
    assert_non_null(a);
    assert_non_null(b);

    auto itA = a->staticFns.find("func");
    assert_true(itA != a->staticFns.end());
    assert_int_equal(1, itA->second.size());
    assert_int_equal(1, itA->second[0].argTypes[0].size());
    assert_true(itA->second[0].argTypes[0].contains((objectType)101));

    auto itB = b->staticFns.find("func");
    assert_true(itB != b->staticFns.end());
    assert_int_equal(1, itB->second.size());
    assert_int_equal(1, itB->second[0].argTypes[0].size());
    assert_true(itB->second[0].argTypes[0].contains((objectType)100));
  });
}

static void test_cross_class_references(void **state) {
  execute_test(test_cross_class_references_impl, state,
               "test_cross_class_references");
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_trivial_memory),
      cmocka_unit_test(test_edn_parser_memory),
      cmocka_unit_test(test_edn_parser_class_parsing_memory),
      cmocka_unit_test(test_class_aliasing),
      cmocka_unit_test(test_static_fields),
      cmocka_unit_test(test_bridge_functions),
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
      cmocka_unit_test(test_class_inheritance_linkage),
      cmocka_unit_test(test_overload_symbols),
      cmocka_unit_test(test_cross_class_references),
      cmocka_unit_test(test_toString),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
