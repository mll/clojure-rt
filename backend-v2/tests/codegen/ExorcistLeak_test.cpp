#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/Function.h"
#include "../../runtime/Var.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_exorcist_leak(void **state) {
    (void)state;
    // (defn exorcist [x y] (+ x y))
    // (exorcist 1N 2N)
    
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        
        // 1. (defn exorcist [x y] (+ x y))
        Node defnNode;
        defnNode.set_op(opDef);
        auto *dn = defnNode.mutable_subnode()->mutable_def();
        dn->set_var("#'user/exorcist");
        
        auto *fnNode = dn->mutable_init();
        fnNode->set_op(opFn);
        auto *fn = fnNode->mutable_subnode()->mutable_fn();
        fn->set_maxfixedarity(1);
        auto *m = fn->add_methods();
        auto *mn = m->mutable_subnode()->mutable_fnmethod();
        mn->set_fixedarity(2);
        mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
        mn->add_params()->mutable_subnode()->mutable_binding()->set_name("y");
        
        auto *body = mn->mutable_body();
        body->set_op(opDo);
        auto *do_ = body->mutable_subnode()->mutable_do_();
        
        auto *stmt = do_->add_statements();
        stmt->set_op(opLocal);
        stmt->mutable_subnode()->mutable_local()->set_name("x");
        stmt->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        
        auto *ret = do_->mutable_ret();
        ret->set_op(opLocal);
        ret->mutable_subnode()->mutable_local()->set_name("y");
        ret->mutable_subnode()->mutable_local()->set_local(localTypeArg);

        // Memory Guidance: After returning 'y', we must release 'x'.
        {
            auto *g1 = ret->add_dropmemory();
            g1->set_variablename("x");
            g1->set_requiredrefcountchange(-1);
        }


        auto defnRes = engine.compileAST(defnNode, "defn_exorcist").get();
        RTValue (*defnFunc)() = defnRes.address.toPtr<RTValue (*)()>();
        RTValue v = defnFunc();
        release(v);

        // 2. (exorcist "hello" "world") - using strings for refcounting
        Node callNode;
        callNode.set_op(opInvoke);
        auto *callInv = callNode.mutable_subnode()->mutable_invoke();
        callInv->mutable_fn()->set_op(opVar);
        callInv->mutable_fn()->mutable_subnode()->mutable_var()->set_var("#'user/exorcist");
        
        auto *a1 = callInv->add_args();
        a1->set_op(opConst);
        a1->mutable_subnode()->mutable_const_()->set_val("1");
        a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        
        auto *a2 = callInv->add_args();
        a2->set_op(opConst);
        a2->mutable_subnode()->mutable_const_()->set_val("2");
        a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);

        auto callRes = engine.compileAST(callNode, "call_exorcist").get();
        RTValue (*callFunc)() = callRes.address.toPtr<RTValue (*)()>();
        
        RTValue res = callFunc();
        release(res);

        engine.retireModule("defn_exorcist");
        engine.retireModule("call_exorcist");
    });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exorcist_leak),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
