#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
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

extern "C" {
RTValue mock_add_bigint(RTValue a, RTValue b) {
  // We don't really care about the result value, just that it's a BigInt
  // that needs to be released.
  release(a);
  release(b);
  return RT_boxPtr(BigInteger_createFromInt(42));
}
}

static void test_let_basic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // (let [x 1 y 2] (+ x y))
    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();

    // Binding x = 1
    auto *bx = let->add_bindings();
    bx->set_op(opBinding);
    auto *bxc = bx->mutable_subnode()->mutable_binding();
    bxc->set_name("x");
    bxc->mutable_init()->set_op(opConst);
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("1");
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    // Binding y = 2
    auto *by = let->add_bindings();
    by->set_op(opBinding);
    auto *byc = by->mutable_subnode()->mutable_binding();
    byc->set_name("y");
    byc->mutable_init()->set_op(opConst);
    byc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("2");
    byc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    // Body: (StaticCall rt.Core/Numbers_add x y)
    auto *body = let->mutable_body();
    body->set_op(opStaticCall);
    auto *sc = body->mutable_subnode()->mutable_staticcall();
    sc->set_class_("rt.Core");
    sc->set_method("Numbers_add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opLocal);
    arg1->mutable_subnode()->mutable_local()->set_name("x");
    arg1->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    auto *arg2 = sc->add_args();
    arg2->set_op(opLocal);
    arg2->mutable_subnode()->mutable_local()->set_name("y");
    arg2->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    // Setup rt.Core/Numbers_add metadata
    auto desc = std::make_unique<rt::ClassDescription>();
    rt::IntrinsicDescription numAdd;
    numAdd.type = rt::CallType::Call;
    numAdd.symbol = "Numbers_add";
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.returnType = rt::ObjectTypeSet::all();
    desc->staticFns["Numbers_add"].push_back(numAdd);

    String *coreName = String_createDynamicStr("rt.Core");
    Ptr_retain(coreName);
    ::Class *coreCls = Class_create(coreName, coreName, 0, nullptr);
    coreCls->compilerExtension = desc.release();
    coreCls->compilerExtensionDestructor = rt::delete_class_description;
    compState.classRegistry.registerObject("rt.Core", coreCls);

    try {
      auto res = engine
                     .compileAST(root, "__test_let_basic")
                     .get()
                     .address;
      RTValue result = res.toPtr<RTValue (*)()>()();
      assert_int_equal(RT_unboxInt32(result), 3);
    } catch (const rt::LanguageException &e) {
      cout << "Caught LanguageException: " << rt::getExceptionString(e) << endl;
      assert_true(false);
    } catch (const std::exception &e) {
      cout << "Caught exception: " << e.what() << endl;
      assert_true(false);
    }
  });
}

static void test_let_memory_mm(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // (let [x "hello"] x)
    // With guidance to retain x twice and release once
    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();

    // Binding x = "hello"
    auto *bx = let->add_bindings();
    bx->set_op(opBinding);
    auto *bxc = bx->mutable_subnode()->mutable_binding();
    bxc->set_name("x");
    bxc->mutable_init()->set_op(opConst);
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("hello");
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeString);

    // Guidance for the binding
    auto *g1 = bx->add_dropmemory();
    g1->set_variablename("x");
    g1->set_requiredrefcountchange(1); // retain x

    // Body: x
    auto *body = let->mutable_body();
    body->set_op(opLocal);
    body->mutable_subnode()->mutable_local()->set_name("x");
    body->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    // Guidance for the body (before body evaluation)
    auto *g2 = root.add_dropmemory();
    g2->set_variablename("x");
    g2->set_requiredrefcountchange(-1); // release x

    cout << "=== Let Memory Guidance Test IR ===" << endl;
    try {
      auto res = engine
                     .compileAST(root, "__test_let_mm")
                     .get()
                     .address;
      cout << "===================================" << endl;

      RTValue result = res.toPtr<RTValue (*)()>()();
      assert_true(RT_isPtr(result));
      String *s = (String *)RT_unboxPtr(result);
      assert_string_equal(String_c_str(s), "hello");
      release(result);
    } catch (const rt::LanguageException &e) {
      cout << "Caught LanguageException: " << rt::getExceptionString(e) << endl;
      assert_true(false);
    } catch (const std::exception &e) {
      cout << "Caught exception: " << e.what() << endl;
      assert_true(false);
    }
  });
}

static void test_let_uaf_fixed(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // (let [x 1N] (+ "aa" x))
    // This used to cause UAF because both let and Numbers_add released x.
    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();

    auto *bx = let->add_bindings();
    bx->set_op(opBinding);
    auto *bxc = bx->mutable_subnode()->mutable_binding();
    bxc->set_name("x");
    bxc->mutable_init()->set_op(opConst);
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("1");
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    bxc->mutable_init()->set_tag("clojure.lang.BigInt");

    auto *body = let->mutable_body();
    body->set_op(opStaticCall);
    auto *sc = body->mutable_subnode()->mutable_staticcall();
    sc->set_class_("rt.Core");
    sc->set_method("Numbers_add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_val("aa");
    arg1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeString);

    auto *arg2 = sc->add_args();
    arg2->set_op(opLocal);
    arg2->mutable_subnode()->mutable_local()->set_name("x");
    arg2->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    // Setup rt.Core/Numbers_add metadata
    auto desc = std::make_unique<rt::ClassDescription>();
    rt::IntrinsicDescription numAdd;
    numAdd.type = rt::CallType::Call;
    numAdd.symbol = "Numbers_add";
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.returnType = rt::ObjectTypeSet::all();
    desc->staticFns["Numbers_add"].push_back(numAdd);

    String *coreName = String_createDynamicStr("rt.Core");
    Ptr_retain(coreName);
    ::Class *coreCls = Class_create(coreName, coreName, 0, nullptr);
    coreCls->compilerExtension = desc.release();
    coreCls->compilerExtensionDestructor = rt::delete_class_description;
    compState.classRegistry.registerObject("rt.Core", coreCls);

    cout << "=== Let UAF Fixed Test (Should throw LanguageException but NOT "
            "crash) ==="
         << endl;
    try {
      {

        auto res = engine
                       .compileAST(root, "__test_let_uaf")
                       .get()
                       .address;
        res.toPtr<RTValue (*)()>()();
        assert_true(false); // Should have thrown
      }
    } catch (const rt::LanguageException &e) {
      cout << "Caught expected LanguageException: " << rt::getExceptionString(e)
           << endl;
    }
  });
}

static void test_let_gap_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // (let [x 1N] (do (+ "aa" "bb") x))
    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();

    auto *bx = let->add_bindings();
    bx->set_op(opBinding);
    auto *bxc = bx->mutable_subnode()->mutable_binding();
    bxc->set_name("x");
    bxc->mutable_init()->set_op(opConst);
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("1");
    bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    bxc->mutable_init()->set_tag("clojure.lang.BigInt");

    auto *body = let->mutable_body();
    body->set_op(opDo);
    auto *do_ = body->mutable_subnode()->mutable_do_();

    auto *stmt = do_->add_statements();
    stmt->set_op(opStaticCall);
    auto *sc = stmt->mutable_subnode()->mutable_staticcall();
    sc->set_class_("rt.Core");
    sc->set_method("Numbers_add");
    auto *a1 = sc->add_args();
    a1->set_op(opConst);
    a1->mutable_subnode()->mutable_const_()->set_val("aa");
    a1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeString);
    auto *a2 = sc->add_args();
    a2->set_op(opConst);
    a2->mutable_subnode()->mutable_const_()->set_val("bb");
    a2->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeString);

    auto *ret = do_->mutable_ret();
    ret->set_op(opLocal);
    ret->mutable_subnode()->mutable_local()->set_name("x");
    ret->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    // Add unwind-memory guidance to simulate real compiler output
    auto *g1 = body->add_unwindmemory();
    g1->set_variablename("x");
    g1->set_requiredrefcountchange(-1);

    auto *g2 = stmt->add_unwindmemory();
    g2->set_variablename("x");
    g2->set_requiredrefcountchange(-1);

    // Setup rt.Core/Numbers_add metadata
    auto desc = std::make_unique<rt::ClassDescription>();
    rt::IntrinsicDescription numAdd;
    numAdd.type = rt::CallType::Call;
    numAdd.symbol = "Numbers_add";
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.returnType = rt::ObjectTypeSet::all();
    desc->staticFns["Numbers_add"].push_back(numAdd);

    String *coreName = String_createDynamicStr("rt.Core");
    Ptr_retain(coreName);
    ::Class *coreCls = Class_create(coreName, coreName, 0, nullptr);
    coreCls->compilerExtension = desc.release();
    coreCls->compilerExtensionDestructor = rt::delete_class_description;
    compState.classRegistry.registerObject("rt.Core", coreCls);

    cout << "=== Let Gap Leak Test (Should NOT leak) ===" << endl;
    try {
      {
        auto res = engine
                       .compileAST(root, "__test_let_leak")
                       .get()
                       .address;
        res.toPtr<RTValue (*)()>()();
        assert_true(false);
      }
    } catch (const rt::LanguageException &e) {
      cout << "Caught expected LanguageException: " << rt::getExceptionString(e)
           << endl;
    }
  });
}

static void test_let_nested_redundancy(void **state) {
  (void)state;
  // We can't use ASSERT_MEMORY_ALL_BALANCED because the JIT will retain the
  // 100N constant but the inner let will release it once. This leads to a net
  // balance of zero if we didn't account for constants. Actually, if we use
  // separate JIT session per test, ASSERT_MEMORY_ALL_BALANCED(engine
  // destruction) should work.

  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // (let [x 100N]
    //   (let [y 200N] 42))
    Node root;
    root.set_op(opLet);
    auto *outerLet = root.mutable_subnode()->mutable_let();

    // Outer binding x = 100N
    auto *bx1 = outerLet->add_bindings();
    bx1->set_op(opBinding);
    auto *bxc1 = bx1->mutable_subnode()->mutable_binding();
    bxc1->set_name("x_outer");
    bxc1->mutable_init()->set_op(opConst);
    bxc1->mutable_init()->mutable_subnode()->mutable_const_()->set_val("100");
    bxc1->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    bxc1->mutable_init()->set_tag("clojure.lang.BigInt");

    // Outer body: Inner Let
    auto *innerNode = outerLet->mutable_body();
    innerNode->set_op(opLet);
    auto *innerLet = innerNode->mutable_subnode()->mutable_let();

    // Guidance for the INNER let node: RELEASE x_outer.
    auto *g = innerNode->add_dropmemory();
    g->set_variablename("x_outer");
    g->set_requiredrefcountchange(-1);

    // Inner binding y = 200
    auto *bx2 = innerLet->add_bindings();
    bx2->set_op(opBinding);
    auto *bxc2 = bx2->mutable_subnode()->mutable_binding();
    bxc2->set_name("y");
    bxc2->mutable_init()->set_op(opConst);
    bxc2->mutable_init()->mutable_subnode()->mutable_const_()->set_val("200");
    bxc2->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    bxc2->mutable_init()->set_tag("long");

    // Body: 42
    auto *innerBody = innerLet->mutable_body();
    innerBody->set_op(opConst);
    innerBody->mutable_subnode()->mutable_const_()->set_val("42");
    innerBody->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    cout << "=== Let Nested Redundancy Detection Test ===" << endl;
    try {
      auto res = engine
                     .compileAST(root, "__test_let_nested_redundancy")
                     .get()
                     .address;
      RTValue val = res.toPtr<RTValue (*)()>()();
      assert_int_equal(42, RT_unboxInt32(val));
    } catch (const std::exception &e) {
      cout << "Caught exception: " << e.what() << endl;
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_let_basic),
      cmocka_unit_test(test_let_memory_mm),
      cmocka_unit_test(test_let_uaf_fixed),
      cmocka_unit_test(test_let_gap_leak),
      cmocka_unit_test(test_let_nested_redundancy),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  return result;
}
