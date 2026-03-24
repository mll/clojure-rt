#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
}
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

RTValue mock_add_bigint(RTValue a, RTValue b) {
  release(a);
  release(b);
  return RT_boxPtr(BigInteger_createFromInt(42));
}
}

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void test_if_truthy_const(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    Node ifNode;
    ifNode.set_op(opIf);
    auto *if_ = ifNode.mutable_subnode()->mutable_if_();

    // Test: true
    auto *test = if_->mutable_test();
    test->set_op(opConst);
    test->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeBool);
    test->mutable_subnode()->mutable_const_()->set_val("true");

    // Then: "then"
    auto *then = if_->mutable_then();
    then->set_op(opConst);
    then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    then->mutable_subnode()->mutable_const_()->set_val("then");

    // Else: "else"
    auto *else_ = if_->mutable_else_();
    else_->set_op(opConst);
    else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    else_->mutable_subnode()->mutable_const_()->set_val("else");

    auto resIf = engine
                     .compileAST(ifNode, "__test_if_truthy_const",
                                 llvm::OptimizationLevel::O0, false)
                     .get().address;

    RTValue result = resPtrToValue(resIf);
    assert_true(RT_isPtr(result));
    assert_string_equal("then", String_c_str(reinterpret_cast<String *>(RT_unboxPtr(result))));
    release(result);
  });
}

static void test_if_falsy_const(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    Node ifNode;
    ifNode.set_op(opIf);
    auto *if_ = ifNode.mutable_subnode()->mutable_if_();

    // Test: false
    auto *test = if_->mutable_test();
    test->set_op(opConst);
    test->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeBool);
    test->mutable_subnode()->mutable_const_()->set_val("false");

    // Then: "then"
    auto *then = if_->mutable_then();
    then->set_op(opConst);
    then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    then->mutable_subnode()->mutable_const_()->set_val("then");

    // Else: "else"
    auto *else_ = if_->mutable_else_();
    else_->set_op(opConst);
    else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    else_->mutable_subnode()->mutable_const_()->set_val("else");

    auto resIf = engine
                     .compileAST(ifNode, "__test_if_falsy_const",
                                 llvm::OptimizationLevel::O0, false)
                     .get().address;

    RTValue result = resPtrToValue(resIf);
    assert_true(RT_isPtr(result));
    assert_string_equal("else", String_c_str(reinterpret_cast<String *>(RT_unboxPtr(result))));
    release(result);
  });
}

static void test_if_nil(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    Node ifNode;
    ifNode.set_op(opIf);
    auto *if_ = ifNode.mutable_subnode()->mutable_if_();

    // Test: nil
    auto *test = if_->mutable_test();
    test->set_op(opConst);
    test->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNil);
    test->mutable_subnode()->mutable_const_()->set_val("nil");

    // Then: "then"
    auto *then = if_->mutable_then();
    then->set_op(opConst);
    then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    then->mutable_subnode()->mutable_const_()->set_val("then");

    // Else: "else"
    auto *else_ = if_->mutable_else_();
    else_->set_op(opConst);
    else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    else_->mutable_subnode()->mutable_const_()->set_val("else");

    auto resIf = engine
                     .compileAST(ifNode, "__test_if_nil",
                                 llvm::OptimizationLevel::O0, false)
                     .get().address;

    RTValue result = resPtrToValue(resIf);
    assert_true(RT_isPtr(result));
    assert_string_equal("else", String_c_str(reinterpret_cast<String *>(RT_unboxPtr(result))));
    release(result);
  });
}

static void test_if_integer_const(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    Node ifNode;
    ifNode.set_op(opIf);
    auto *if_ = ifNode.mutable_subnode()->mutable_if_();

    // Test: 1
    auto *test = if_->mutable_test();
    test->set_op(opConst);
    test->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    test->set_tag("long");
    test->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    test->mutable_subnode()->mutable_const_()->set_val("1");

    // Then: "then"
    auto *then = if_->mutable_then();
    then->set_op(opConst);
    then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    then->mutable_subnode()->mutable_const_()->set_val("then");

    // Else: "else"
    auto *else_ = if_->mutable_else_();
    else_->set_op(opConst);
    else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    else_->mutable_subnode()->mutable_const_()->set_val("else");

    auto resIf = engine
                     .compileAST(ifNode, "__test_if_integer_const",
                                 llvm::OptimizationLevel::O0, false)
                     .get().address;

    RTValue result = resPtrToValue(resIf);
    assert_true(RT_isPtr(result));
    assert_string_equal("then", String_c_str(reinterpret_cast<String *>(RT_unboxPtr(result))));
    release(result);
  });
}

static void test_if_test_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;

    // (if (+ 1N 2N) 42 43)
    Node root;
    root.set_op(opIf);
    auto *if_ = root.mutable_subnode()->mutable_if_();

    auto *testNode = if_->mutable_test();
    testNode->set_op(opStaticCall);
    auto *sc = testNode->mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");
    
    auto *a1 = sc->add_args();
    a1->set_op(opConst);
    a1->mutable_subnode()->mutable_const_()->set_val("1");
    a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    a1->set_tag("clojure.lang.BigInt");

    auto *a2 = sc->add_args();
    a2->set_op(opConst);
    a2->mutable_subnode()->mutable_const_()->set_val("2");
    a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    a2->set_tag("clojure.lang.BigInt");

    auto *then = if_->mutable_then();
    then->set_op(opConst);
    then->mutable_subnode()->mutable_const_()->set_val("42");
    then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    then->set_tag("long");

    auto *else_ = if_->mutable_else_();
    else_->set_op(opConst);
    else_->mutable_subnode()->mutable_const_()->set_val("43");
    else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    else_->set_tag("long");

    // Setup clojure.lang.Numbers/add metadata
    auto desc = std::make_unique<rt::ClassDescription>();
    rt::IntrinsicDescription numAdd;
    numAdd.type = rt::CallType::Call;
    numAdd.symbol = "mock_add_bigint"; 
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.returnType = rt::ObjectTypeSet::all();
    desc->staticFns["add"].push_back(numAdd);

    String *numName = String_createDynamicStr("clojure.lang.Numbers");
    Ptr_retain(numName);
    ::Class *numCls = Class_create(numName, numName, 0, nullptr);
    numCls->compilerExtension = desc.release();
    numCls->compilerExtensionDestructor = rt::delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", numCls);

    cout << "=== If Test Leak Test (Should NOT leak) ===" << endl;
    try {
      rt::JITEngine engine(compState);
      auto res = engine.compileAST(root, "__test_if_leak", llvm::OptimizationLevel::O0, true).get().address;
      
      RTValue val = res.toPtr<RTValue (*)()>()();
      assert_int_equal(42, RT_unboxInt32(val));
    } catch (const std::exception &e) {
      cout << "Caught exception: " << e.what() << endl;
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_if_truthy_const),
      cmocka_unit_test(test_if_falsy_const),
      cmocka_unit_test(test_if_nil),
      cmocka_unit_test(test_if_integer_const),
      cmocka_unit_test(test_if_test_leak),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
