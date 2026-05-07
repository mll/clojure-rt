#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Ebr.h"
#include "../../runtime/Numbers.h"
#include "../../runtime/String.h"
#include "../../tools/EdnParser.h"
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void delete_class_description(void *ptr) {
  delete static_cast<rt::ClassDescription *>(ptr);
}

static void setup_minimal_runtime(JITEngine &engine) {
  auto &compState = engine.threadsafeState;

  {
    String *nameStr = String_create("clojure.lang.Numbers");
    Ptr_retain(nameStr);
    ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    rt::ClassDescription *ext = new rt::ClassDescription();
    ext->name = "clojure.lang.Numbers";
    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = ::delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
  }

  {
    String *nameStr = String_create("java.math.BigInteger");
    Ptr_retain(nameStr);
    ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    rt::ClassDescription *ext = new rt::ClassDescription();
    ext->name = "java.math.BigInteger";
    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = ::delete_class_description;
    compState.classRegistry.registerObject("java.math.BigInteger", cls);
  }
}

static void test_variadic_basic(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine(llvm::OptimizationLevel::O0, false);
    setup_minimal_runtime(engine);

    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_isvariadic(true);
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->set_isvariadic(true);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x__#0");
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name(
        "args__#0");
    auto *body = mn->mutable_body();
    body->set_op(opLocal);
    body->mutable_subnode()->mutable_local()->set_name("args__#0");
    body->mutable_subnode()->mutable_local()->set_local(localTypeArg);

    auto *gBodyX = body->add_dropmemory();
    gBodyX->set_variablename("x__#0");
    gBodyX->set_requiredrefcountchange(-1);

    auto *uBodyArgs = body->add_unwindmemory();
    uBodyArgs->set_variablename("args__#0");
    uBodyArgs->set_requiredrefcountchange(-1);

    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();
    auto *bf = let->add_bindings();
    bf->set_op(opBinding);
    auto *bnF = bf->mutable_subnode()->mutable_binding();
    bnF->set_name("f__#0");
    bnF->mutable_init()->CopyFrom(fnNode);

    for (int i = 1; i <= 3; ++i) {
      auto *ba = let->add_bindings();
      ba->set_op(opBinding);
      auto *b = ba->mutable_subnode()->mutable_binding();
      b->set_name("v" + std::to_string(i) + "__#0");
      b->mutable_init()->set_op(opConst);
      b->mutable_init()->mutable_subnode()->mutable_const_()->set_val(
          std::to_string(i));
      b->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeNumber);
    }

    auto *lb = let->mutable_body();
    lb->set_op(opDo);
    auto *do_ = lb->mutable_subnode()->mutable_do_();

    auto *invNode = do_->mutable_ret();
    invNode->set_op(opInvoke);
    auto *inv = invNode->mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->set_op(opLocal);
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f__#0");
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);

    auto *uFn = inv->mutable_fn()->add_unwindmemory();
    uFn->set_variablename("f__#0");
    uFn->set_requiredrefcountchange(-1);

    for (int i = 1; i <= 3; ++i) {
      auto *a = inv->add_args();
      a->set_op(opLocal);
      a->mutable_subnode()->mutable_local()->set_name("v" + std::to_string(i) +
                                                      "__#0");
      a->mutable_subnode()->mutable_local()->set_local(localTypeLet);

      auto *uArg = a->add_unwindmemory();
      uArg->set_variablename("f__#0");
      uArg->set_requiredrefcountchange(-1);
    }

    // No drop-memory for f__#0 or vN here because they are consumed by the
    // InvokeNode backend and the variadic pack respectively.

    auto res = engine.compileAST(root, "test1").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_int_equal(persistentListType, getType(result));
    PersistentList *list = (PersistentList *)RT_unboxPtr(result);
    assert_int_equal(2, RT_unboxInt32(list->first));
    assert_int_equal(3, RT_unboxInt32(list->rest->first));
    release(result);
  });
}

static void test_variadic_only_bigints(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine(llvm::OptimizationLevel::O0, false);

    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_isvariadic(true);
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(0);
    mn->set_isvariadic(true);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name(
        "args__#0");
    mn->mutable_body()->set_op(opLocal);
    mn->mutable_body()->mutable_subnode()->mutable_local()->set_name(
        "args__#0");
    mn->mutable_body()->mutable_subnode()->mutable_local()->set_local(
        localTypeArg);

    auto *uBodyArgs = mn->mutable_body()->add_unwindmemory();
    uBodyArgs->set_variablename("args__#0");
    uBodyArgs->set_requiredrefcountchange(-1);

    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();
    auto *bf = let->add_bindings();
    bf->set_op(opBinding);
    auto *bnF = bf->mutable_subnode()->mutable_binding();
    bnF->set_name("f__#0");
    bnF->mutable_init()->CopyFrom(fnNode);

    for (int i = 0; i < 5; ++i) {
      auto *ba = let->add_bindings();
      ba->set_op(opBinding);
      auto *b = ba->mutable_subnode()->mutable_binding();
      b->set_name("arg" + std::to_string(i) + "__#0");
      b->mutable_init()->set_op(opConst);
      b->mutable_init()->mutable_subnode()->mutable_const_()->set_val(
          std::to_string(i));
      b->mutable_init()->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeNumber);
      b->mutable_init()->set_tag("clojure.lang.BigInt");
    }

    auto *lb = let->mutable_body();
    lb->set_op(opDo);
    auto *do_ = lb->mutable_subnode()->mutable_do_();
    auto *invNode = do_->mutable_ret();
    invNode->set_op(opInvoke);
    auto *inv = invNode->mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->set_op(opLocal);
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f__#0");
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);

    auto *uFn = inv->mutable_fn()->add_unwindmemory();
    uFn->set_variablename("f__#0");
    uFn->set_requiredrefcountchange(-1);

    for (int i = 0; i < 5; ++i) {
      auto *a = inv->add_args();
      a->set_op(opLocal);
      a->mutable_subnode()->mutable_local()->set_name(
          "arg" + std::to_string(i) + "__#0");
      a->mutable_subnode()->mutable_local()->set_local(localTypeLet);

      auto *uArg = a->add_unwindmemory();
      uArg->set_variablename("f__#0");
      uArg->set_requiredrefcountchange(-1);
    }

    // Consumption handled by backend and bouncer.

    auto res = engine.compileAST(root, "test2").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_int_equal(persistentListType, getType(result));
    PersistentList *curr = (PersistentList *)RT_unboxPtr(result);
    for (int i = 0; i < 5; ++i) {
      assert_int_equal(bigIntegerType, getType(curr->first));
      assert_int_equal(
          i, mpz_get_si(((BigInteger *)RT_unboxPtr(curr->first))->value));
      curr = curr->rest;
    }
    release(result);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_variadic_basic),
      cmocka_unit_test(test_variadic_only_bigints),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
