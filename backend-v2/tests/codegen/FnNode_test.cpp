#include "../../codegen/CodeGen.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "../../types/ConstantInteger.h"
#include "bytecode.pb.h"

#include <iostream>

#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/RTValue.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/defines.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void setup_mock_runtime(rt::ThreadsafeCompilerState &compState) {
  // Add "Add" intrinsic to clojure.lang.Numbers
  String *nameStr = String_createDynamicStr("clojure.lang.Numbers");
  Ptr_retain(nameStr);
  Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

  ClassDescription *ext = new ClassDescription();
  ext->name = "clojure.lang.Numbers";

  IntrinsicDescription addId;
  addId.symbol = "Add";
  addId.type = CallType::Intrinsic;
  addId.argTypes = {ObjectTypeSet(integerType, false),
                    ObjectTypeSet(integerType, false)};
  addId.returnType = ObjectTypeSet(integerType, false);
  ext->staticFns["add"].push_back(addId);

  cls->compilerExtension = ext;
  cls->compilerExtensionDestructor = rt::delete_class_description;
  compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
}

// (fn [x] x)
static void test_simple_fn(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    fn->set_once(false);
    fn->set_maxfixedarity(1);

    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->set_isvariadic(false);

    auto *param = mn->add_params();
    param->set_op(opBinding);
    param->mutable_subnode()->mutable_binding()->set_name("x");

    auto *body = mn->mutable_body();
    body->set_op(opLocal);
    body->mutable_subnode()->mutable_local()->set_name("x");

    auto res =
        engine
            .compileAST(fnNode, "simple_fn", llvm::OptimizationLevel::O0, true)
            .get();

    // Execute: returns a ClojureFunction object
    RTValue funObj = res.address.toPtr<RTValue (*)()>()();
    assert_true(RT_isPtr(funObj));
    assert_int_equal(functionType, ::getType(funObj));

    ClojureFunction *f = (ClojureFunction *)RT_unboxPtr(funObj);
    assert_int_equal(1, f->methodCount);

    // We can't easily call the closure from here without a frame setup,
    // but the fact it compiled and returned a function object is a good start.

    release(funObj);
  });
}

// (let [u 10] (fn [x] (+ x u)))
static void test_fn_capture_unboxing_int(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    setup_mock_runtime(engine.threadsafeState);

    Node letNode;
    letNode.set_op(opLet);
    auto *let = letNode.mutable_subnode()->mutable_let();

    auto *b = let->add_bindings();
    b->set_op(opBinding);
    auto *bn = b->mutable_subnode()->mutable_binding();
    bn->set_name("u");
    auto *val = bn->mutable_init();
    val->set_op(opConst);
    val->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    val->mutable_subnode()->mutable_const_()->set_val("10");
    val->set_tag("long"); // Ensure it's unboxed long

    auto *bodyFn = let->mutable_body();
    bodyFn->set_op(opFn);
    auto *fn = bodyFn->mutable_subnode()->mutable_fn();
    fn->set_maxfixedarity(1);

    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);

    auto *p1 = mn->add_params();
    p1->set_op(opBinding);
    p1->mutable_subnode()->mutable_binding()->set_name("x");

    // Captured 'u'
    auto *co = mn->add_closedovers();
    co->set_op(opLocal);
    co->mutable_subnode()->mutable_local()->set_name("u");

    // (+ x u)
    auto *body = mn->mutable_body();
    body->set_op(opStaticCall);
    auto *sc = body->mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opLocal);
    arg1->mutable_subnode()->mutable_local()->set_name("x");

    auto *arg2 = sc->add_args();
    arg2->set_op(opLocal);
    arg2->mutable_subnode()->mutable_local()->set_name("u");

    cout << "=== Fn Capture Unboxing Test IR ===" << endl;
    auto res = engine
                   .compileAST(letNode, "fn_capture_int",
                               llvm::OptimizationLevel::O0, true)
                   .get();
    cout << "====================================" << endl;

    RTValue funObj = res.address.toPtr<RTValue (*)()>()();
    assert_true(RT_isPtr(funObj));

    ClojureFunction *f = (ClojureFunction *)RT_unboxPtr(funObj);
    assert_int_equal(1, f->methods[0].closedOversCount);

    // The IR dump (if viewed) should show unbox_int for %u%

    release(funObj);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_simple_fn),
      cmocka_unit_test(test_fn_capture_unboxing_int),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
