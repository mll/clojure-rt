#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_mock_runtime(rt::ThreadsafeCompilerState &compState) {
    {
        String *nameStr = String_create("TestClass");
        Ptr_retain(nameStr);
        Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
        
        ClassDescription *ext = new ClassDescription();
        ext->name = "TestClass";
        
        // Literal constant in metadata
        ext->staticFields["constInt"] = RT_boxInt32(123);
        ext->staticFieldIndices["constInt"] = 0;
        ext->staticFieldValues.push_back(RT_boxInt32(123));
        
        ext->staticFields["constDouble"] = RT_boxDouble(45.6);
        ext->staticFieldIndices["constDouble"] = 1;
        ext->staticFieldValues.push_back(RT_boxDouble(45.6));

        ext->staticFields["constBool"] = RT_boxBool(true);
        ext->staticFieldIndices["constBool"] = 2;
        ext->staticFieldValues.push_back(RT_boxBool(true));

        // Runtime lookup if not in staticFields (though it's in metadata indices)
        ext->staticFieldIndices["runtimeInt"] = 3;
        ext->staticFieldValues.push_back(RT_boxInt32(999));

        cls->compilerExtension = ext;
        cls->compilerExtensionDestructor = delete_class_description;
        compState.classRegistry.registerObject("TestClass", cls);
    }
}

static void test_static_field_const_int(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    JITEngine engine(compState);

    Node node;
    node.set_op(opStaticField);
    auto *sf = node.mutable_subnode()->mutable_staticfield();
    sf->set_class_("TestClass");
    sf->set_field("constInt");

    try {
        auto res = engine.compileAST(node, "__test_sf_int", llvm::OptimizationLevel::O0, true).get().address;
        RTValue result = resPtrToValue(res);
        assert_true(RT_isInt32(result));
        assert_int_equal(123, RT_unboxInt32(result));
        release(result);
    } catch (const LanguageException &e) {
        fprintf(stderr, "LanguageException: %s\n", getExceptionString(e).c_str());
        assert_true(false);
    } catch (const std::exception &e) {
        fprintf(stderr, "std::exception: %s\n", e.what());
        assert_true(false);
    }
}

static void test_static_field_const_double(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    JITEngine engine(compState);

    Node node;
    node.set_op(opStaticField);
    auto *sf = node.mutable_subnode()->mutable_staticfield();
    sf->set_class_("TestClass");
    sf->set_field("constDouble");

    try {
        auto res = engine.compileAST(node, "__test_sf_double", llvm::OptimizationLevel::O0, true).get().address;
        RTValue result = resPtrToValue(res);
        assert_true(RT_isDouble(result));
        assert_double_equal(45.6, RT_unboxDouble(result), 0.001);
        release(result);
    } catch (const LanguageException &e) {
        fprintf(stderr, "LanguageException: %s\n", getExceptionString(e).c_str());
        assert_true(false);
    }
}

static void test_static_field_const_bool(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    JITEngine engine(compState);

    Node node;
    node.set_op(opStaticField);
    auto *sf = node.mutable_subnode()->mutable_staticfield();
    sf->set_class_("TestClass");
    sf->set_field("constBool");

    try {
        auto res = engine.compileAST(node, "__test_sf_bool", llvm::OptimizationLevel::O0, true).get().address;
        RTValue result = resPtrToValue(res);
        assert_true(RT_isBool(result));
        assert_true(RT_unboxBool(result));
        release(result);
    } catch (const LanguageException &e) {
        fprintf(stderr, "LanguageException: %s\n", getExceptionString(e).c_str());
        assert_true(false);
    }
}

static void test_static_field_runtime_lookup(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    JITEngine engine(compState);

    Node node;
    node.set_op(opStaticField);
    auto *sf = node.mutable_subnode()->mutable_staticfield();
    sf->set_class_("TestClass");
    sf->set_field("runtimeInt");

    try {
        auto res = engine.compileAST(node, "__test_sf_runtime", llvm::OptimizationLevel::O0, true).get().address;
        RTValue result = resPtrToValue(res);
        assert_true(RT_isInt32(result));
        assert_int_equal(999, RT_unboxInt32(result));
        release(result);
    } catch (const LanguageException &e) {
        fprintf(stderr, "LanguageException: %s\n", getExceptionString(e).c_str());
        assert_true(false);
    }
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_static_field_const_int),
        cmocka_unit_test(test_static_field_const_double),
        cmocka_unit_test(test_static_field_const_bool),
        cmocka_unit_test(test_static_field_runtime_lookup),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);
    RuntimeInterface_cleanup();
    return result;
}
