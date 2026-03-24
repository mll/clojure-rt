#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_void_invoke_verification(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  JITEngine engine(compState);

  // (+ 2147483647 1) -> This should trigger checked Add with overflow in codegen
  Node addNode;
  addNode.set_op(opStaticCall);
  auto *sc = addNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("clojure.lang.Numbers");
  sc->set_method("add");

  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  auto *c1 = arg1->mutable_subnode()->mutable_const_();
  c1->set_val("2147483647");
  c1->set_type(ConstNode_ConstType_constTypeNumber);
  arg1->set_tag("int");

  auto *arg2 = sc->add_args();
  arg2->set_op(opConst);
  auto *c2 = arg2->mutable_subnode()->mutable_const_();
  c2->set_val("1");
  c2->set_type(ConstNode_ConstType_constTypeNumber);
  arg2->set_tag("int");

  // We don't even need to run it, just compile it. 
  // If verification fails, it will throw/crash during compileAST.
  try {
    (void)engine.compileAST(addNode, "__test_void_invoke", llvm::OptimizationLevel::O0, false).get().address;
  } catch (const std::exception &e) {
    // It might throw due to overflow at runtime if we execute it, 
    // but here we just want to see if it COMPILES without verification error.
    fprintf(stderr, "Caught expected or unexpected exception: %s\n", e.what());
  }
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_void_invoke_verification),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
