#include "../../codegen/CodeGen.h"
#include "../../runtime/Function.h"
#include "../../runtime/Keyword.h"
#include "../../runtime/String.h"
#include "../../runtime/Var.h"
#include "jit/JITEngine.h"
#include "runtime/Ebr.h"
#include "runtime/RTValue.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include <llvm/Support/Error.h>

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static Node create_identity_fn() {
  Node fnNode;
  fnNode.set_op(opFn);
  auto *fn = fnNode.mutable_subnode()->mutable_fn();
  fn->set_maxfixedarity(1);
  auto *m = fn->add_methods();
  auto *mn = m->mutable_subnode()->mutable_fnmethod();
  mn->set_fixedarity(1);
  auto *p = mn->add_params();
  p->set_op(opBinding);
  p->mutable_subnode()->mutable_binding()->set_name("x");

  auto *body = mn->mutable_body();
  body->set_op(opLocal);
  auto *ln = body->mutable_subnode()->mutable_local();
  ln->set_name("x");
  ln->set_local(localTypeArg);
  return fnNode;
}

static Node create_constant_fn(int value) {
  Node fnNode;
  fnNode.set_op(opFn);
  auto *fn = fnNode.mutable_subnode()->mutable_fn();
  fn->set_maxfixedarity(1);
  auto *m = fn->add_methods();
  auto *mn = m->mutable_subnode()->mutable_fnmethod();
  mn->set_fixedarity(1);
  mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");

  auto *body = mn->mutable_body();
  body->set_op(opConst);
  auto *c = body->mutable_subnode()->mutable_const_();
  c->set_type(ConstNode_ConstType_constTypeNumber);
  c->set_val(std::to_string(value));
  return fnNode;
}

static void test_var_call_ic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    auto &compState = engine.threadsafeState;

    // 1. Create a Var and bind a function to it
    RTValue kw = Keyword_create(String_create("user/my-fn"));
    Var *v = Var_create(kw);
    Ptr_retain(v);
    compState.varRegistry.registerObject("user/my-fn", v);

    // Function 1: Identity
    Node fn1Node = create_identity_fn();
    auto fn1Res =
        engine.compileAST(fn1Node, "fn1", llvm::OptimizationLevel::O0, true)
            .get();
    RTValue (*fn1Ctor)() = fn1Res.address.toPtr<RTValue (*)()>();
    RTValue fn1 = fn1Ctor();
    Ptr_retain(v);
    Var_bindRoot(v, fn1);

    // 2. Create an InvokeNode calling #'user/my-fn
    Node invokeNode;
    invokeNode.set_op(opInvoke);
    auto *inv = invokeNode.mutable_subnode()->mutable_invoke();

    auto *fnPart = inv->mutable_fn();
    fnPart->set_op(opVar);
    fnPart->mutable_subnode()->mutable_var()->set_var("#'user/my-fn");

    auto *arg1 = inv->add_args();
    arg1->set_op(opConst);
    auto *c = arg1->mutable_subnode()->mutable_const_();
    c->set_type(ConstNode_ConstType_constTypeNumber);
    c->set_val("42");

    // 3. Compile and Run
    auto res = engine
                   .compileAST(invokeNode, "var_call_test",
                               llvm::OptimizationLevel::O0, true)
                   .get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();

    {
      // First call: IC miss, slow path
      RTValue r1 = func();
      assert_int_equal(42, RT_unboxInt32(r1));

      // Second call: IC hit, fast path
      RTValue r2 = func();
      assert_int_equal(42, RT_unboxInt32(r2));

      // 4. Change the Var root to another function
      Node fn2Node = create_constant_fn(100);
      auto fn2Res =
          engine.compileAST(fn2Node, "fn2", llvm::OptimizationLevel::O0, true)
              .get();
      RTValue (*fn2Ctor)() = fn2Res.address.toPtr<RTValue (*)()>();
      RTValue fn2 = fn2Ctor();

      Ptr_retain(v);
      Var_bindRoot(v, fn2);

      // 5. Call again: IC miss due to change, update IC, slow path
      RTValue r3 = func();
      assert_int_equal(100, RT_unboxInt32(r3));

      RTValue r4 = func();
      assert_int_equal(100, RT_unboxInt32(r4));
    }

    // 6. Cleanup

    Var_unbindRoot(v);

    engine.retireModule("var_call_test");
    engine.retireModule("fn1");
    engine.retireModule("fn2");
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_call_ic),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
