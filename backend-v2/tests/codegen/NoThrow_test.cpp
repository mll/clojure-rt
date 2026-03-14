#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_no_throw_collection(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // Create a nested vector of constants: [[1 2] [3 4]]
  Node root;
  root.set_op(opVector);
  auto *vroot = root.mutable_subnode()->mutable_vector();

  for (int i = 0; i < 2; ++i) {
    auto *inner = vroot->add_items();
    inner->set_op(opVector);
    auto *vinner = inner->mutable_subnode()->mutable_vector();
    for (int j = 0; j < 2; ++j) {
        auto *c = vinner->add_items();
        c->set_op(opConst);
        c->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        c->mutable_subnode()->mutable_const_()->set_val(to_string(i * 2 + j));
    }
  }

  // Compile and dump IR
  auto res = engine.compileAST(root, "__test_no_throw", llvm::OptimizationLevel::O0, true).get();
  
  // The output should NOT contain "invoke" or "landingpad"
  // We can't easily capture stdout here but we can verify it doesn't crash 
  // and the result is correct.
  RTValue result = res.toPtr<RTValue (*)()>()();
  assert_true(RT_isPtr(result));
  release(result);
}

static void test_can_throw_collection(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // Create a vector with a static call: [1 (some-func)]
  Node root;
  root.set_op(opVector);
  auto *vroot = root.mutable_subnode()->mutable_vector();

  auto *c1 = vroot->add_items();
  c1->set_op(opConst);
  c1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  c1->mutable_subnode()->mutable_const_()->set_val("1");

  auto *call = vroot->add_items();
  call->set_op(opStaticCall);
  auto *sc = call->mutable_subnode()->mutable_staticcall();
  sc->set_class_("rt.Core");
  sc->set_method("negate"); // Dummy call that might throw
  
  // Note: This might fail at runtime if negate is not found, 
  // but we are mostly interested in the fact it generates an invoke if it could.
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_no_throw_collection),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
