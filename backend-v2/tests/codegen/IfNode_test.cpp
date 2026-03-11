#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

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
                     .get();

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
                     .get();

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
                     .get();

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
                     .get();

    RTValue result = resPtrToValue(resIf);
    assert_true(RT_isPtr(result));
    assert_string_equal("then", String_c_str(reinterpret_cast<String *>(RT_unboxPtr(result))));
    release(result);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_if_truthy_const),
      cmocka_unit_test(test_if_falsy_const),
      cmocka_unit_test(test_if_nil),
      cmocka_unit_test(test_if_integer_const),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
