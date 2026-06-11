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

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_hostinterop_compile(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    Node interopNode;
    interopNode.set_op(opHostInterop);
    auto *hi = interopNode.mutable_subnode()->mutable_hostinterop();

    // Target: A constant string
    auto *target = hi->mutable_target();
    target->set_op(opConst);
    target->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    target->mutable_subnode()->mutable_const_()->set_val("hello");

    // Morf: some method
    hi->set_morf("length");

    auto res = engine.compileAST(interopNode, "__test_hostinterop").get().address;

    bool caught = false;
    try {
      res.toPtr<RTValue (*)()>()();
    } catch (const LanguageException& e) {
      // It should throw because we didn't implement length or it's not registered
      caught = true;
    } catch (...) {
      caught = true;
    }
    assert_true(caught);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_hostinterop_compile),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
