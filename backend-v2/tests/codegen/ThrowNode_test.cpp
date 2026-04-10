#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <iostream>
#include <stdlib.h>

extern "C" {
#include "../../runtime/Exception.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_throw_exception_bridge(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    RTValue msg = RT_boxPtr(String_createDynamicStr("test message"));

    // In our runtime, Exception objects wrap LanguageException
    LanguageException *lePtr =
        new LanguageException("AssertionError", msg, RT_boxNil());

    Exception *ex = (Exception *)malloc(sizeof(Exception));
    Object_create((Object *)ex, exceptionType);
    ex->bridgedData = lePtr;

    RTValue boxedEx = RT_boxPtr(ex);

    bool caught = false;
    try {
      throwException_C(boxedEx);
    } catch (const LanguageException &e) {
      caught = true;
      assert_string_equal("AssertionError", e.getName().c_str());
    } catch (...) {
      assert_true(false); // Should not catch other types
    }

    assert_true(caught);
  });
}

static void test_codegen_throw(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // (throw "error")
    Node throwNode;
    throwNode.set_op(opThrow);
    auto *tr = throwNode.mutable_subnode()->mutable_throw_();

    auto *ex = tr->mutable_exception();
    ex->set_op(opConst);
    ex->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeString);
    ex->mutable_subnode()->mutable_const_()->set_val("error");

    bool caught = false;
    try {
      // We expect this to throw an "InternalInconsistencyException" (as a
      // LanguageException) because "error" is a String, not an Exception
      // object.
      auto resThrow = engine
                          .compileAST(throwNode, "__test_codegen_throw",
                                      llvm::OptimizationLevel::O0, false)
                          .get()
                          .address;

      resThrow.toPtr<RTValue (*)()>()();
    } catch (const LanguageException &e) {
      caught = true;
      assert_string_equal("CodeGenerationException", e.getName().c_str());
    } catch (...) {
      caught = true;
    }
    assert_true(caught);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_throw_exception_bridge),
      cmocka_unit_test(test_codegen_throw),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);

  return result;
}
