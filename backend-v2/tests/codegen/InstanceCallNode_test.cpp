#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
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

// Mock implementation for instance call
extern "C" {
int32_t mock_instance_get_value(RTValue instance, int32_t offset) {
  // We can't easily check 'instance' here without a full class setup,
  // but we can verify the argument passing and that it was called.
  return 100 + offset;
}
RTValue mock_Vector_pop(RTValue instance) {
  // Mock pop: just return the same instance for test simplicity
  retain(instance);
  return instance;
}
}

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void
setup_mock_instance_metadata(rt::ThreadsafeCompilerState &compState) {
  // MyClass with instance method 'getValue'
  {
    String *nameStr = String_create("MyClass");
    Ptr_retain(nameStr); // for className
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

    ClassDescription *ext = new ClassDescription();
    ext->name = "MyClass";

    // Value matches 'MyClass'
    ext->type = ObjectTypeSet(integerType);

    // Instance method: (getValue [this int] -> int)
    IntrinsicDescription getValue;
    getValue.symbol = "mock_instance_get_value";
    getValue.type = CallType::Call;
    getValue.argTypes.push_back(ObjectTypeSet(integerType));        // this
    getValue.argTypes.push_back(ObjectTypeSet(integerType, false)); // arg1
    getValue.returnType = ObjectTypeSet(integerType, false);

    ext->instanceFns["getValue"].push_back(getValue);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    Ptr_retain(cls);
    compState.classRegistry.registerObject("java.lang.Integer", cls);
    compState.classRegistry.registerObject(cls, (int32_t)integerType);
  }

  // Vector class with instance method 'pop'
  {
    String *nameStr = String_create("clojure.lang.PersistentVector");
    Ptr_retain(nameStr); // for className
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

    ClassDescription *ext = new ClassDescription();
    ext->name = "clojure.lang.PersistentVector";
    ext->type = ObjectTypeSet(persistentVectorType);

    IntrinsicDescription pop;
    pop.symbol = "mock_Vector_pop";
    pop.type = CallType::Call;
    pop.argTypes.push_back(
        ObjectTypeSet(persistentVectorType)); // :this should resolve to this
    pop.returnType = ObjectTypeSet(persistentVectorType);

    ext->instanceFns["pop"].push_back(pop);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    Ptr_retain(cls); // Retain for the second registration
    compState.classRegistry.registerObject("Vector", cls);
    compState.classRegistry.registerObject(cls, (int32_t)persistentVectorType);
    // Both references (original and retained) are consumed
  }
}

static void test_static_instance_call(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_instance_metadata(compState);
    JITEngine engine(compState);

    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("getValue");

    // 1. Instance (statically known as integerType/Int)
    auto *inst = ic->mutable_instance();
    inst->set_op(opConst);
    inst->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    inst->mutable_subnode()->mutable_const_()->set_val("0");
    inst->set_tag("long");

    // 2. Arg (int)
    auto *arg = ic->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("42");
    arg->set_tag("long");

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_static_instance_call",
                                     llvm::OptimizationLevel::O0, true)
                         .get().address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(142, RT_unboxInt32(result));
      release(result);
    } catch (const rt::LanguageException &e) {
      fprintf(stderr, "CodeGenerationException in test: %s\n",
              rt::getExceptionString(e).c_str());
      assert_true(false);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception in test: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_vector_pop_call(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_instance_metadata(compState);
    JITEngine engine(compState);

    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("pop");

    // Instance: Vector
    auto *inst = ic->mutable_instance();
    inst->set_op(opConst);
    inst->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeVector);
    // Vector literal: empty for this test
    inst->mutable_subnode()->mutable_const_()->set_val("[]");

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_vector_pop",
                                     llvm::OptimizationLevel::O0, true)
                         .get().address;
      RTValue result = resPtrToValue(resCall);
      assert_int_equal(persistentVectorType, getType(result));
      release(result);
    } catch (const rt::LanguageException &e) {
      fprintf(stderr, "CodeGenerationException in Vector.pop test: %s\n",
              rt::getExceptionString(e).c_str());
      assert_true(false);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception in Vector.pop test: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_dynamic_instance_call(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_instance_metadata(compState);
    JITEngine engine(compState);

    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("getValue");

    // 1. Instance (statically known as Int)
    auto *inst = ic->mutable_instance();
    inst->set_op(opConst);
    inst->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    inst->mutable_subnode()->mutable_const_()->set_val("0");
    inst->set_tag("long");

    // 2. Arg (dynamic/untagged, but at runtime it will be Int)
    auto *arg = ic->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("42");
    // NO TAG -> Dynamic dispatch

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_dynamic_instance_call",
                                     llvm::OptimizationLevel::O0, true)
                         .get().address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(142, RT_unboxInt32(result));
      release(result);
    } catch (const rt::LanguageException &e) {
      fprintf(stderr, "CodeGenerationException in dynamic test: %s\n",
              rt::getExceptionString(e).c_str());
      assert_true(false);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception in dynamic test: %s\n", e.what());
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_static_instance_call),
      cmocka_unit_test(test_vector_pop_call),
      cmocka_unit_test(test_dynamic_instance_call),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
