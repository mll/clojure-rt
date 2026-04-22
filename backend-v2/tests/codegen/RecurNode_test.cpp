#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/Function.h"
#include "../../tools/EdnParser.h"
#include "../../state/ThreadsafeCompilerState.h"
#include <fstream>
#include <iostream>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void setup_compiler_state(rt::ThreadsafeCompilerState &compState,
                                 rt::JITEngine &engine) {
  Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  llvm::orc::ExecutorAddr resClasses =
      engine.compileAST(astClasses.nodes(0), "__classes").get().address;
  RTValue classes = resClasses.toPtr<RTValue (*)()>()();
  auto classesList = rt::buildClasses(classes);
  for (auto &desc : classesList) {
    auto &nameStr = desc->name;
    String *className = String_create(nameStr.c_str());
    Ptr_retain(className);
    Class *cls = Class_create(className, className, 0, nullptr);
    cls->compilerExtension = desc.release();
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(nameStr.c_str(), cls);
  }
}

static void test_recur_factorial_bigint(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        setup_compiler_state(engine.threadsafeState, engine);
        
        // (fn [n acc] (if (equiv 0 n) acc (recur (dec n) (* n acc))))
        Node fnNode;
        fnNode.set_op(opFn);
        auto *fn = fnNode.mutable_subnode()->mutable_fn();
        fn->set_maxfixedarity(2);
        auto *m = fn->add_methods();
        auto *mn = m->mutable_subnode()->mutable_fnmethod();
        mn->set_fixedarity(2);
        mn->set_loopid("fact_loop");
        
        auto *pn = mn->add_params();
        pn->set_op(opBinding);
        pn->mutable_subnode()->mutable_binding()->set_name("n");
        
        auto *pacc = mn->add_params();
        pacc->set_op(opBinding);
        pacc->mutable_subnode()->mutable_binding()->set_name("acc");
        
        auto *body = mn->mutable_body();
        body->set_op(opIf);
        auto *ifn = body->mutable_subnode()->mutable_if_();
        
        // Test: (clojure.lang.Util/equiv 0 n)
        auto *test = ifn->mutable_test();
        test->set_op(opStaticCall);
        auto *sc = test->mutable_subnode()->mutable_staticcall();
        sc->set_class_("clojure.lang.Util");
        sc->set_method("equiv");
        
        auto *arg0 = sc->add_args();
        arg0->set_op(opConst);
        arg0->mutable_subnode()->mutable_const_()->set_val("0");
        arg0->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        
        auto *argn = sc->add_args();
        argn->set_op(opLocal);
        argn->mutable_subnode()->mutable_local()->set_name("n");
        argn->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        
        // Then: acc
        auto *then = ifn->mutable_then();
        then->set_op(opLocal);
        then->mutable_subnode()->mutable_local()->set_name("acc");
        then->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        
        // Else: (recur (dec n) (* n acc))
        auto *else_ = ifn->mutable_else_();
        else_->set_op(opRecur);
        auto *rn = else_->mutable_subnode()->mutable_recur();
        rn->set_loopid("fact_loop");
        
        // arg1: (clojure.lang.Numbers/minus n 1)
        auto *rarg1 = rn->add_exprs();
        rarg1->set_op(opStaticCall);
        auto *sc1 = rarg1->mutable_subnode()->mutable_staticcall();
        sc1->set_class_("clojure.lang.Numbers");
        sc1->set_method("minus");
        auto *sc1_arg1 = sc1->add_args();
        sc1_arg1->set_op(opLocal);
        sc1_arg1->mutable_subnode()->mutable_local()->set_name("n");
        sc1_arg1->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        auto *sc1_arg2 = sc1->add_args();
        sc1_arg2->set_op(opConst);
        sc1_arg2->mutable_subnode()->mutable_const_()->set_val("1");
        sc1_arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        
        // arg2: (clojure.lang.Numbers/multiply n acc)
        auto *rarg2 = rn->add_exprs();
        rarg2->set_op(opStaticCall);
        auto *sc2 = rarg2->mutable_subnode()->mutable_staticcall();
        sc2->set_class_("clojure.lang.Numbers");
        sc2->set_method("multiply");
        auto *sc2_arg1 = sc2->add_args();
        sc2_arg1->set_op(opLocal);
        sc2_arg1->mutable_subnode()->mutable_local()->set_name("n");
        sc2_arg1->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        auto *sc2_arg2 = sc2->add_args();
        sc2_arg2->set_op(opLocal);
        sc2_arg2->mutable_subnode()->mutable_local()->set_name("acc");
        sc2_arg2->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        
        // (let [fact (fn [n acc] ...)] (fact 5 1N))
        Node letNode;
        letNode.set_op(opLet);
        auto *let = letNode.mutable_subnode()->mutable_let();
        
        auto *bf = let->add_bindings();
        bf->set_op(opBinding);
        bf->mutable_subnode()->mutable_binding()->set_name("fact");
        bf->mutable_subnode()->mutable_binding()->mutable_init()->CopyFrom(fnNode);

        auto *letBody = let->mutable_body();
        letBody->set_op(opInvoke);
        auto *inv = letBody->mutable_subnode()->mutable_invoke();
        inv->mutable_fn()->set_op(opLocal);
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("fact");
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);

        // arg1: 5
        auto *arg1 = inv->add_args();
        arg1->set_op(opConst);
        arg1->mutable_subnode()->mutable_const_()->set_val("5");
        arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        
        // arg2: 1N
        auto *arg2 = inv->add_args();
        arg2->set_op(opConst);
        arg2->mutable_subnode()->mutable_const_()->set_val("1");
        arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        arg2->set_tag("clojure.lang.BigInt");

        auto res = engine.compileAST(letNode, "recur_fact_test").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        
        assert_int_equal(bigIntegerType, getType(result));
        BigInteger *resultBI = (BigInteger *)RT_unboxPtr(result);
        assert_int_equal(120, mpz_get_si(resultBI->value));
        
        release(result);
    });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_recur_factorial_bigint),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
