#include <atomic>
#include <iostream>
#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/Function.h"
#include "../../runtime/PersistentList.h"
#include "../../bridge/Exceptions.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Helper to create a (fn [x] x) node
static void create_identity_fn(Node& fnNode) {
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(1);
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    auto *p = mn->add_params();
    p->mutable_subnode()->mutable_binding()->set_name("x");
    auto *body = mn->mutable_body();
    body->set_op(opLocal);
    auto *ln = body->mutable_subnode()->mutable_local();
    ln->set_name("x");
    ln->set_local(localTypeArg);
}

// (let [f (fn [x] x)] (f 42)) -> Dynamic Invoke because 'f' is a local
static void test_dynamic_invoke_simple(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        
        Node letNode;
        letNode.set_op(opLet);
        auto *let = letNode.mutable_subnode()->mutable_let();
        
        // binding f = (fn [x] x)
        auto *b = let->add_bindings();
        auto *bn = b->mutable_subnode()->mutable_binding();
        bn->set_name("f");
        create_identity_fn(*bn->mutable_init());
        
        // body = (f 42)
        auto *body = let->mutable_body();
        body->set_op(opInvoke);
        auto *inv = body->mutable_subnode()->mutable_invoke();
        auto *fLocal = inv->mutable_fn();
        fLocal->set_op(opLocal);
        fLocal->mutable_subnode()->mutable_local()->set_name("f");
        fLocal->mutable_subnode()->mutable_local()->set_local(localTypeLet);
        
        auto *arg = inv->add_args();
        arg->set_op(opConst);
        arg->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        arg->mutable_subnode()->mutable_const_()->set_val("42");
        
        auto res = engine.compileAST(letNode, "dynamic_invoke_simple", llvm::OptimizationLevel::O0, true).get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        
        assert_int_equal(42, RT_unboxInt32(result));
    });
}

// (defn caller [f x] (f x)) -> Calling 'f' is dynamic
static void test_dynamic_invoke_higher_order(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;

        // AST: (let [caller (fn [f x] (f x))] (caller (fn [y] y) 100))
        Node outerLet;
        outerLet.set_op(opLet);
        auto *let = outerLet.mutable_subnode()->mutable_let();

        // caller = (fn [f x] (f x))
        {
            auto *b = let->add_bindings();
            auto *bn = b->mutable_subnode()->mutable_binding();
            bn->set_name("caller");
            auto *cFnNode = bn->mutable_init();
            cFnNode->set_op(opFn);
            auto *cFn = cFnNode->mutable_subnode()->mutable_fn();
            cFn->set_maxfixedarity(2);
            auto *m = cFn->add_methods();
            auto *mn = m->mutable_subnode()->mutable_fnmethod();
            mn->set_fixedarity(2);
            mn->add_params()->mutable_subnode()->mutable_binding()->set_name("f");
            mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
            
            auto *body = mn->mutable_body();
            body->set_op(opInvoke);
            auto *inv = body->mutable_subnode()->mutable_invoke();
            inv->mutable_fn()->set_op(opLocal);
            inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
            inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeArg);
            auto *a = inv->add_args();
            a->set_op(opLocal);
            a->mutable_subnode()->mutable_local()->set_name("x");
            a->mutable_subnode()->mutable_local()->set_local(localTypeArg);
        }

        // body = (caller (fn [y] y) 100)
        {
            auto *body = let->mutable_body();
            body->set_op(opInvoke);
            auto *inv = body->mutable_subnode()->mutable_invoke();
            inv->mutable_fn()->set_op(opLocal);
            inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("caller");
            inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
            
            create_identity_fn(*inv->add_args());
            
            auto *a2 = inv->add_args();
            a2->set_op(opConst);
            a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
            a2->mutable_subnode()->mutable_const_()->set_val("100");
        }

        auto res = engine.compileAST(outerLet, "dynamic_ho_test", llvm::OptimizationLevel::O0, true).get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        assert_int_equal(100, RT_unboxInt32(result));
    });
}

// Multi-arity dynamic matching
static void test_dynamic_invoke_multi_arity(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
         rt::JITEngine engine;
         
         // (let [f (fn ([x] 1) ([x y] 2))] (f 10 20))
         Node letNode;
         letNode.set_op(opLet);
         auto *let = letNode.mutable_subnode()->mutable_let();
         
         auto *b = let->add_bindings();
         auto *bn = b->mutable_subnode()->mutable_binding();
         bn->set_name("f");
         auto *fnNode = bn->mutable_init();
         fnNode->set_op(opFn);
         auto *fn = fnNode->mutable_subnode()->mutable_fn();
         {
             auto *m1 = fn->add_methods();
             auto *mn1 = m1->mutable_subnode()->mutable_fnmethod();
             mn1->set_fixedarity(1);
             mn1->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
             mn1->mutable_body()->set_op(opConst);
             mn1->mutable_body()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
             mn1->mutable_body()->mutable_subnode()->mutable_const_()->set_val("1");
             
             auto *m2 = fn->add_methods();
             auto *mn2 = m2->mutable_subnode()->mutable_fnmethod();
             mn2->set_fixedarity(2);
             mn2->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
             mn2->add_params()->mutable_subnode()->mutable_binding()->set_name("y");
             mn2->mutable_body()->set_op(opConst);
             mn2->mutable_body()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
             mn2->mutable_body()->mutable_subnode()->mutable_const_()->set_val("2");
         }
         
         auto *body = let->mutable_body();
         body->set_op(opInvoke);
         auto *inv = body->mutable_subnode()->mutable_invoke();
         inv->mutable_fn()->set_op(opLocal);
         inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
         inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
         
         inv->add_args()->set_op(opConst); // Dummy arg1
         inv->add_args()->set_op(opConst); // Dummy arg2
         
         auto res = engine.compileAST(letNode, "dynamic_multi_arity", llvm::OptimizationLevel::O0, true).get();
         RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
         RTValue result = func();
         assert_int_equal(2, RT_unboxInt32(result));
    });
}

// Variadic dynamic packing
static void test_dynamic_invoke_variadic(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
         rt::JITEngine engine;
         
         // (let [f (fn [x & args] args)] (f 1 2 3)) -> returns (2 3)
         Node letNode;
         letNode.set_op(opLet);
         auto *let = letNode.mutable_subnode()->mutable_let();
         
         auto *b = let->add_bindings();
         auto *bn = b->mutable_subnode()->mutable_binding();
         bn->set_name("f");
         auto *fnNode = bn->mutable_init();
         fnNode->set_op(opFn);
         auto *fn = fnNode->mutable_subnode()->mutable_fn();
         fn->set_isvariadic(true);
         auto *mn = fn->add_methods()->mutable_subnode()->mutable_fnmethod();
         mn->set_fixedarity(1);
         mn->set_isvariadic(true);
         mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
         mn->add_params()->mutable_subnode()->mutable_binding()->set_name("args");
         mn->mutable_body()->set_op(opLocal);
         mn->mutable_body()->mutable_subnode()->mutable_local()->set_name("args");
         mn->mutable_body()->mutable_subnode()->mutable_local()->set_local(localTypeArg);
         
         auto *body = let->mutable_body();
         body->set_op(opInvoke);
         auto *inv = body->mutable_subnode()->mutable_invoke();
         inv->mutable_fn()->set_op(opLocal);
         inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
         inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
         
         for(int i=1; i<=3; ++i) {
             auto *a = inv->add_args();
             a->set_op(opConst);
             a->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
             a->mutable_subnode()->mutable_const_()->set_val(std::to_string(i));
         }
         
         auto res = engine.compileAST(letNode, "dynamic_variadic", llvm::OptimizationLevel::O0, true).get();
         RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
         RTValue result = func();
         
         // result should be (2 3)
         assert_int_equal(persistentListType, getType(result));
         PersistentList *list = (PersistentList*)RT_unboxPtr(result);
         printf("Variadic result refcount: %lu\n", (unsigned long)list->super.atomicRefCount);
         assert_int_equal(2, RT_unboxInt32(list->first));
         assert_true(list->rest != NULL);
         assert_int_equal(3, RT_unboxInt32(list->rest->first));
         release(result); 
    });
}

// Arity exception in dynamic path
static void test_dynamic_invoke_arity_exception(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        
        // (let [f (fn [x] x)] (f 1 2)) -> ArityException
        Node letNode;
        letNode.set_op(opLet);
        auto *let = letNode.mutable_subnode()->mutable_let();
        auto *bn = let->add_bindings()->mutable_subnode()->mutable_binding();
        bn->set_name("f");
        create_identity_fn(*bn->mutable_init());
        
        auto *body = let->mutable_body();
        body->set_op(opInvoke);
        auto *inv = body->mutable_subnode()->mutable_invoke();
        inv->mutable_fn()->set_op(opLocal);
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
        inv->add_args()->set_op(opConst);
        inv->add_args()->set_op(opConst);
        
        auto res = engine.compileAST(letNode, "dynamic_arity_ex", llvm::OptimizationLevel::O0, true).get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        
        bool caught = false;
        try {
            func();
        } catch (const rt::LanguageException& e) {
            if (e.getName() == "ArityException") {
                caught = true;
            }
        }
        assert_true(caught);
    });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_dynamic_invoke_simple),
      cmocka_unit_test(test_dynamic_invoke_higher_order),
      cmocka_unit_test(test_dynamic_invoke_multi_arity),
      cmocka_unit_test(test_dynamic_invoke_variadic),
      cmocka_unit_test(test_dynamic_invoke_arity_exception),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
