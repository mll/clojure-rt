#include <atomic>
#include <iostream>
#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/Function.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_static_invoke_simple(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        
        // AST: ((fn [x] x) 42)
        Node invokeNode;
        invokeNode.set_op(opInvoke);
        auto *inv = invokeNode.mutable_subnode()->mutable_invoke();
        
        // fn = (fn [x] x)
        auto *fnNode = inv->mutable_fn();
        fnNode->set_op(opFn);
        auto *fn = fnNode->mutable_subnode()->mutable_fn();
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

        // arg1 = 42
        auto *arg1 = inv->add_args();
        arg1->set_op(opConst);
        auto *c = arg1->mutable_subnode()->mutable_const_();
        c->set_type(ConstNode_ConstType_constTypeNumber);
        c->set_val("42");
        
        auto res = engine.compileAST(invokeNode, "static_invoke_test").get();
        
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        
        assert_int_equal(42, RT_unboxInt32(result));
    });
}

// (fn ([x] x) ([x y] y))
static void test_static_invoke_multi_arity(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(2);
    
    // Arity 1: (fn [x] x)
    {
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
    }
    
    // Arity 2: (fn [x y] y)
    {
      auto *m = fn->add_methods();
      auto *mn = m->mutable_subnode()->mutable_fnmethod();
      mn->set_fixedarity(2);
      auto *p1 = mn->add_params();
      p1->set_op(opBinding);
      p1->mutable_subnode()->mutable_binding()->set_name("x");
      auto *p2 = mn->add_params();
      p2->set_op(opBinding);
      p2->mutable_subnode()->mutable_binding()->set_name("y");
      auto *body = mn->mutable_body();
      body->set_op(opLocal);
      auto *ln = body->mutable_subnode()->mutable_local();
      ln->set_name("y");
      ln->set_local(localTypeArg);
    }

    // Test Calling Arity 1
    {
        Node invNode;
        invNode.set_op(opInvoke);
        auto *inv = invNode.mutable_subnode()->mutable_invoke();
        inv->mutable_fn()->CopyFrom(fnNode);
        auto *a1 = inv->add_args();
        a1->set_op(opConst);
        a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        a1->mutable_subnode()->mutable_const_()->set_val("10");
        
        auto res = engine.compileAST(invNode, "invoke_arity1").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        assert_int_equal(10, RT_unboxInt32(result));
    }

    // Test Calling Arity 2
    {
        Node invNode;
        invNode.set_op(opInvoke);
        auto *inv = invNode.mutable_subnode()->mutable_invoke();
        inv->mutable_fn()->CopyFrom(fnNode);
        auto *a1 = inv->add_args();
        a1->set_op(opConst);
        a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        a1->mutable_subnode()->mutable_const_()->set_val("10");
        auto *a2 = inv->add_args();
        a2->set_op(opConst);
        a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        a2->mutable_subnode()->mutable_const_()->set_val("20");
        
        auto res = engine.compileAST(invNode, "invoke_arity2").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = func();
        assert_int_equal(20, RT_unboxInt32(result));
    }
    });
}

static void test_static_invoke_arity_mismatch_leak(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
         rt::JITEngine engine;
         
         // AST: ((fn [x] x) 1 2) -> ArityException
         Node invokeNode;
         invokeNode.set_op(opInvoke);
         auto *inv = invokeNode.mutable_subnode()->mutable_invoke();
         
         // fn = (fn [x] x)
         auto *fnNode = inv->mutable_fn();
         fnNode->set_op(opFn);
         auto *fn = fnNode->mutable_subnode()->mutable_fn();
         fn->set_maxfixedarity(1);
         auto *mn = fn->add_methods()->mutable_subnode()->mutable_fnmethod();
         mn->set_fixedarity(1);
         mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
         mn->mutable_body()->set_op(opLocal);
         mn->mutable_body()->mutable_subnode()->mutable_local()->set_name("x");
         mn->mutable_body()->mutable_subnode()->mutable_local()->set_local(localTypeArg);
         
         inv->add_args()->set_op(opConst);
         inv->add_args()->set_op(opConst);
         
         bool caught = false;
         try {
             auto res = engine.compileAST(invokeNode, "static_arity_leak").get();
             fail_msg("Expected CodeGenerationException but compilation succeeded");
         } catch (const rt::LanguageException& e) {
             if (e.getName() == "CodeGenerationException") {
                 caught = true;
             } else {
                 fail_msg("Expected CodeGenerationException but caught %s", e.getName().c_str());
             }
         }
         assert_true(caught);
    });
}

static void test_dynamic_invoke_arity_mismatch_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    auto &compState = engine.threadsafeState;

    // 1. Create a Var and bind a 1-arity function to it
    const char *varName = "user/dynamic-arity-fn";
    RTValue kw = Keyword_create(String_create(varName));
    Var *v = Var_create(kw);
    Ptr_retain(v);
    Ptr_retain(v);
    compState.varRegistry.registerObject(varName, v);

    // fn = (fn [x] x)
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(1);
    auto *mn = fn->add_methods()->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
    mn->mutable_body()->set_op(opLocal);
    mn->mutable_body()->mutable_subnode()->mutable_local()->set_name("x");
    mn->mutable_body()->mutable_subnode()->mutable_local()->set_local(localTypeArg);

    auto compiledFnRes = engine.compileAST(fnNode, "dynamic_fn").get();
    RTValue (*fnBuilder)() = compiledFnRes.address.toPtr<RTValue (*)()>();
    RTValue funcObj = fnBuilder();
    
    Var_bindRoot(v, funcObj);

    // 2. Create an InvokeNode calling #'user/dynamic-arity-fn with 2 args
    Node invokeNode;
    invokeNode.set_op(opInvoke);
    auto *inv = invokeNode.mutable_subnode()->mutable_invoke();

    auto *fnPart = inv->mutable_fn();
    fnPart->set_op(opVar);
    fnPart->mutable_subnode()->mutable_var()->set_var(std::string("#'") + varName);

    inv->add_args()->set_op(opConst);
    inv->add_args()->set_op(opConst);

    // 3. Compile and Run
    auto res = engine.compileAST(invokeNode, "dynamic_arity_test").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();

    bool caught = false;
    try {
      func();
    } catch (const rt::LanguageException &e) {
      if (e.getName() == "ArityException") {
        caught = true;
      }
    }
    assert_true(caught);

    // 4. Cleanup
    Ptr_release(v);
    engine.retireModule("dynamic_fn");
    engine.retireModule("dynamic_arity_test");
  });
}

static void test_static_invoke_overflow(void **state) {
    (void)state;
    rt::JITEngine engine;
    
    // fn = (fn [x1 x2 x3 x4 x5 x6 x7] x7)
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(7);
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(7);
    for (int i = 1; i <= 7; ++i) {
        auto *p = mn->add_params();
        p->set_op(opBinding);
        p->mutable_subnode()->mutable_binding()->set_name("x" + std::to_string(i));
    }
    auto *body = mn->mutable_body();
    body->set_op(opLocal);
    auto *ln = body->mutable_subnode()->mutable_local();
    ln->set_name("x7");
    ln->set_local(localTypeArg);

    // Call: ((fn ...) 1 2 3 4 5 6 7)
    Node invNode;
    invNode.set_op(opInvoke);
    auto *inv = invNode.mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->CopyFrom(fnNode);
    for (int i = 1; i <= 7; ++i) {
        auto *a = inv->add_args();
        a->set_op(opConst);
        a->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        a->mutable_subnode()->mutable_const_()->set_val(std::to_string(i * 10));
    }
    
    auto res = engine.compileAST(invNode, "invoke_overflow").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue result = func();
    assert_int_equal(70, RT_unboxInt32(result));
}

static void test_var_call_unbound(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    auto &compState = engine.threadsafeState;

    // 1. Create a Var but do NOT bind it
    const char *varName = "user/unbound-fn";
    RTValue kw = Keyword_create(String_create(varName));
    Var *v = Var_create(kw);
    Ptr_retain(v);
    compState.varRegistry.registerObject(varName, v);

    // 2. Create an InvokeNode calling #'user/unbound-fn
    Node invokeNode;
    invokeNode.set_op(opInvoke);
    auto *inv = invokeNode.mutable_subnode()->mutable_invoke();

    auto *fnPart = inv->mutable_fn();
    fnPart->set_op(opVar);
    fnPart->mutable_subnode()->mutable_var()->set_var(std::string("#'") +
                                                     varName);

    auto *arg1 = inv->add_args();
    arg1->set_op(opConst);
    auto *c = arg1->mutable_subnode()->mutable_const_();
    c->set_type(ConstNode_ConstType_constTypeNumber);
    c->set_val("42");

    // 3. Compile and Run
    auto res = engine.compileAST(invokeNode, "var_call_unbound_test").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();

    bool caught = false;
    try {
      // This should throw because the var is unbound
      func();
    } catch (const rt::LanguageException &e) {
      caught = true;
      // The user's edit in Var.c throws IllegalStateException
      assert_string_equal("IllegalStateException", e.getName().c_str());
    } catch (...) {
      fail_msg("Expected LanguageException but caught something else");
    }

    assert_true(caught);

    // 4. Cleanup
    Ptr_release(v);
    engine.retireModule("var_call_unbound_test");
  });
}
static void test_static_keyword_invoke_on_number(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;

    // AST: (:foo 1)
    Node root;
    root.set_op(opKeywordInvoke);
    auto *ki = root.mutable_subnode()->mutable_keywordinvoke();
    
    auto *kw = ki->mutable_keyword();
    kw->set_op(opConst);
    kw->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
    kw->mutable_subnode()->mutable_const_()->set_val(":foo");

    auto *target = ki->mutable_target();
    target->set_op(opConst);
    target->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    target->mutable_subnode()->mutable_const_()->set_val("1");

    auto res = engine.compileAST(root, "keyword_on_number").get().address;
    RTValue (*func)() = res.toPtr<RTValue (*)()>();
    RTValue result = func();

    // Result should be nil according to Clojure semantics
    assert_true(RT_isNil(result));
  });
}

static void test_static_keyword_invoke_on_number_with_default(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;

    // AST: (:foo 1 :default)
    Node root;
    root.set_op(opInvoke);
    auto *inv = root.mutable_subnode()->mutable_invoke();
    
    auto *kw = inv->mutable_fn();
    kw->set_op(opConst);
    kw->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
    kw->mutable_subnode()->mutable_const_()->set_val(":foo");

    auto *arg1 = inv->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("1");

    auto *arg2 = inv->add_args();
    arg2->set_op(opConst);
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
    arg2->mutable_subnode()->mutable_const_()->set_val(":default");

    auto res = engine.compileAST(root, "keyword_on_number_default").get().address;
    RTValue (*func)() = res.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_true(RT_isKeyword(result));
    String *s = Keyword_toString(result);
    s = String_compactify(s);
    assert_string_equal(String_c_str(s), ":default");
    Ptr_release(s);
    release(result);
  });
}

static void test_dynamic_keyword_invoke_on_number(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;

    RTValue kwVal = Keyword_create(String_create("foo"));
    Var *v = Var_create(kwVal);
    Ptr_retain(v);
    Var_bindRoot(v, kwVal); // Var_bindRoot releases v once, so we retained it above
    Ptr_retain(v);          // Retain again so the registry and our manual release both have one
    engine.threadsafeState.varRegistry.registerObject("v_kw", v);

    Node root;
    root.set_op(opInvoke);
    auto *inv = root.mutable_subnode()->mutable_invoke();
    
    auto *fn = inv->mutable_fn();
    fn->set_op(opVar);
    fn->mutable_subnode()->mutable_var()->set_var("#'v_kw");

    auto *arg = inv->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("1");

    auto res = engine.compileAST(root, "dynamic_keyword_on_number").get().address;
    RTValue (*func)() = res.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_true(RT_isNil(result));
    Ptr_release(v);
    engine.retireModule("dynamic_keyword_on_number");
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_static_invoke_simple),
      cmocka_unit_test(test_static_invoke_multi_arity),
      cmocka_unit_test(test_static_invoke_overflow),
      cmocka_unit_test(test_static_invoke_arity_mismatch_leak),
      cmocka_unit_test(test_dynamic_invoke_arity_mismatch_leak),
      cmocka_unit_test(test_var_call_unbound),
      cmocka_unit_test(test_static_keyword_invoke_on_number),
      cmocka_unit_test(test_static_keyword_invoke_on_number_with_default),
      cmocka_unit_test(test_dynamic_keyword_invoke_on_number),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
