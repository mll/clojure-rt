#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/BigInteger.h"
#include "../../runtime/Function.h"
#include "../../runtime/Object.h"
#include "../../runtime/Var.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_var_call_leak_repro(void **state) {
  (void)state;
  // We want to see if memory is balanced.
  // The user's code:
  // (defn castigator [x & z] (+ x 3N) z)
  // (castigator 5N 5N 2N)

  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // 1. (defn castigator [x & z] (+ x 3N) z)
    Node defnNode;
    defnNode.set_op(opDef);
    auto *dn = defnNode.mutable_subnode()->mutable_def();
    dn->set_var("#'user/castigator");

    auto *fnNode = dn->mutable_init();
    fnNode->set_op(opFn);
    auto *fn = fnNode->mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(1);
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->set_isvariadic(true);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("z");

    // Body: (do (+ x 3) z)
    auto *body = mn->mutable_body();
    body->set_op(opDo);
    auto *do_ = body->mutable_subnode()->mutable_do_();

    // (+ x 3) - using Const "42" as placeholder for the dead expression
    auto *plus = do_->add_statements();
    plus->set_op(opConst);
    plus->mutable_subnode()->mutable_const_()->set_val("42");
    plus->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    // z (variadic arg)
    auto *ret = do_->mutable_ret();
    ret->set_op(opLocal);
    ret->mutable_subnode()->mutable_local()->set_name("z");
    ret->mutable_subnode()->mutable_local()->set_local(localTypeArg);

    // Add Memory Guidance: Drop 'x' before returning 'z'
    auto *guidance = ret->add_dropmemory();
    guidance->set_variablename("x");
    guidance->set_requiredrefcountchange(-1);

    printf("Compiling defn...\n");
    auto defnRes = engine
                       .compileAST(defnNode, "defn_castigator")
                       .get();
    RTValue (*defnFunc)() = defnRes.address.toPtr<RTValue (*)()>();
    RTValue v = defnFunc();
    // The result of defn is the Var. We should release it as it's the top
    // level.
    release(v);

    // 2. (castigator 5N 5N 2N)
    Node callNode;
    callNode.set_op(opInvoke);
    auto *inv = callNode.mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->set_op(opVar);
    inv->mutable_fn()->mutable_subnode()->mutable_var()->set_var(
        "#'user/castigator");

    for (int i = 0; i < 3; i++) {
      auto *a = inv->add_args();
      a->set_op(opConst);
      a->mutable_subnode()->mutable_const_()->set_val("hello");
      a->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeString);
    }

    printf("Compiling call...\n");
    auto callRes = engine
                       .compileAST(callNode, "call_castigator")
                       .get();
    RTValue (*callFunc)() = callRes.address.toPtr<RTValue (*)()>();

    RTValue res = callFunc();
    printf("Result obtained.\n");
    release(res);

    engine.retireModule("defn_castigator");
    engine.retireModule("call_castigator");
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_var_call_leak_repro),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
