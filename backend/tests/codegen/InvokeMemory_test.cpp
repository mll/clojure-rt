#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Ebr.h"
#include "../../runtime/Numbers.h"
#include "../../runtime/String.h"
#include "../../tools/EdnParser.h"
#include <iostream>
#include <memory>

#include "../../runtime/Exception.h"
#include "../../runtime/Object.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>

void delete_class_description(void *ptr);
void *createException_C(const char *className, String *message,
                        RTValue payload);
String *String_create(const char *s);
RTValue Exception_create();
}

extern "C" RTValue Exception_create() {
  String *msg = String_create("");
  ::Exception *self = (::Exception *)allocate(sizeof(::Exception));
  Object_create((Object *)self, exceptionType);
  self->bridgedData = createException_C("java.lang.Exception", msg, 0);
  return RT_boxPtr(self);
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Minimal runtime setup to avoid leaks in the full EDN-based loader
static void setup_minimal_runtime(JITEngine &engine) {
  auto &compState = engine.threadsafeState;

  {
    String *nameStr = String_create("clojure.lang.Numbers");
    Ptr_retain(nameStr);
    ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    ClassDescription *ext = new ClassDescription();
    ext->name = "clojure.lang.Numbers";
    IntrinsicDescription add;
    add.symbol = "Numbers_add";
    add.type = rt::CallType::Call;
    add.argTypes = {ObjectTypeSet::all(), ObjectTypeSet::all()};
    add.returnType = ObjectTypeSet::all();
    ext->staticFns["add"].push_back(add);
    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = ::delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
  }

  {
    String *nameStr = String_create("java.lang.Exception");
    Ptr_retain(nameStr);
    ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    cls->registerId = exceptionType;
    ClassDescription *ext = new ClassDescription();
    ext->name = "java.lang.Exception";

    // Add default constructor
    IntrinsicDescription ctor;
    ctor.symbol = "Exception_create";
    ctor.type = rt::CallType::Call;
    ctor.argTypes = {};
    ctor.returnType = ObjectTypeSet(exceptionType).boxed();
    ext->constructors.push_back(ctor);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = ::delete_class_description;
    Ptr_retain(cls);
    compState.classRegistry.registerObject(cls, cls->registerId);
    compState.classRegistry.registerObject("java.lang.Exception", cls);
  }
}

// Test Case 1: Functions with Captures
static void test_invoke_captures_memory(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine(llvm::OptimizationLevel::O0, false);
    setup_minimal_runtime(engine);

    // (let [x 10N] (let [f (fn [y] (+ x y))] (f 5)))
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("y");
    mn->add_closedovers()->set_op(opLocal);
    mn->mutable_closedovers(0)->mutable_subnode()->mutable_local()->set_name(
        "x");
    mn->mutable_closedovers(0)->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);

    auto *plus = mn->mutable_body();
    plus->set_op(opStaticCall);
    auto *sc = plus->mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");
    sc->add_args()->set_op(opLocal);
    sc->mutable_args(0)->mutable_subnode()->mutable_local()->set_name("x");
    sc->mutable_args(0)->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);
    sc->add_args()->set_op(opLocal);
    sc->mutable_args(1)->mutable_subnode()->mutable_local()->set_name("y");
    sc->mutable_args(1)->mutable_subnode()->mutable_local()->set_local(
        localTypeArg);

    auto *gPlus = plus->add_dropmemory();
    gPlus->set_variablename("x");
    gPlus->set_requiredrefcountchange(1);

    auto *gUnwindFn = fnNode.add_unwindmemory();
    gUnwindFn->set_variablename("x");
    gUnwindFn->set_requiredrefcountchange(-1);

    Node root;
    root.set_op(opLet);
    auto *letX = root.mutable_subnode()->mutable_let();
    auto *bx = letX->add_bindings();
    bx->set_op(opBinding);
    bx->mutable_subnode()->mutable_binding()->set_name("x");
    auto *ia1 = bx->mutable_subnode()->mutable_binding()->mutable_init();
    ia1->set_op(opConst);
    ia1->mutable_subnode()->mutable_const_()->set_val("10");
    ia1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    ia1->set_tag("clojure.lang.BigInt");

    auto *letF_Node = letX->mutable_body();
    letF_Node->set_op(opLet);
    auto *letF = letF_Node->mutable_subnode()->mutable_let();
    auto *bf = letF->add_bindings();
    bf->set_op(opBinding);
    bf->mutable_subnode()->mutable_binding()->set_name("f");
    bf->mutable_subnode()->mutable_binding()->mutable_init()->CopyFrom(fnNode);

    auto *do_Node = letF->mutable_body();
    do_Node->set_op(opDo);
    auto *do_ = do_Node->mutable_subnode()->mutable_do_();

    auto *invNode = do_->mutable_ret();
    invNode->set_op(opInvoke);
    auto *inv = invNode->mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->set_op(opLocal);
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
    inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);
    auto *arg = inv->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_val("5");
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    // [NEW] Aligning with dump: invoke has unwind-memory for f
    auto *gUnwindF = invNode->add_unwindmemory();
    gUnwindF->set_variablename("f");
    gUnwindF->set_requiredrefcountchange(-1);

    auto res = engine.compileAST(root, "test1").get();
    cout << res.optimizedIR << endl;
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_int_equal(bigIntegerType, getType(result));
    assert_int_equal(15,
                     mpz_get_si(((BigInteger *)RT_unboxPtr(result))->value));
    release(result);
  });
}

// Test Case 2: Exception Unwinding
static void test_invoke_exception_unwinding(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine(llvm::OptimizationLevel::O0, false);
    setup_minimal_runtime(engine);
    // (let [f (fn [x y] (throw (new java.lang.Exception)))]
    //  (f 10 (f 20 30)))
    Node throwerFn;
    throwerFn.set_op(opFn);
    auto *fn = throwerFn.mutable_subnode()->mutable_fn();
    auto *mn = fn->add_methods()->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(2);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("x__#0");
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("y__#0");
    auto *body = mn->mutable_body();
    body->set_op(opThrow);
    auto *dmX = body->add_dropmemory();
    dmX->set_variablename("x__#0");
    dmX->set_requiredrefcountchange(-1);
    auto *dmY = body->add_dropmemory();
    dmY->set_variablename("y__#0");
    dmY->set_requiredrefcountchange(-1);
    auto *ex = body->mutable_subnode()->mutable_throw_()->mutable_exception();
    ex->set_op(opNew);
    auto *nn = ex->mutable_subnode()->mutable_new_();
    nn->mutable_class_()->set_op(opConst);
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_val(
        "java.lang.Exception");

    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();
    auto *bn = let->add_bindings()->mutable_subnode()->mutable_binding();
    bn->set_name("f__#0");
    bn->mutable_init()->CopyFrom(throwerFn);

    auto *inv = let->mutable_body();
    inv->set_op(opInvoke);
    auto *i = inv->mutable_subnode()->mutable_invoke();
    i->mutable_fn()->set_op(opLocal);
    i->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f__#0");
    i->mutable_fn()->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);

    auto *uFn = i->mutable_fn()->add_unwindmemory();
    uFn->set_variablename("f__#0");
    uFn->set_requiredrefcountchange(-1);
    auto *dFn = i->mutable_fn()->add_dropmemory();
    dFn->set_variablename("f__#0");
    dFn->set_requiredrefcountchange(1);

    auto *arg1 = i->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_val("10");
    arg1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg1->set_tag("clojure.lang.BigInt");

    auto *uArg1 = arg1->add_unwindmemory();
    uArg1->set_variablename("f__#0");
    uArg1->set_requiredrefcountchange(-1);

    auto *arg2 = i->add_args();
    arg2->set_op(opInvoke);
    auto *i2 = arg2->mutable_subnode()->mutable_invoke();
    i2->mutable_fn()->set_op(opLocal);
    i2->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f__#0");
    i2->mutable_fn()->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);
    auto *uFn2 = i2->mutable_fn()->add_unwindmemory();
    uFn2->set_variablename("f__#0");
    uFn2->set_requiredrefcountchange(-1);
    auto *a2_1 = i2->add_args();
    a2_1->set_op(opConst);
    a2_1->mutable_subnode()->mutable_const_()->set_val("20");
    a2_1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    a2_1->set_tag("clojure.lang.BigInt");
    auto *a2_2 = i2->add_args();
    a2_2->set_op(opConst);
    a2_2->mutable_subnode()->mutable_const_()->set_val("30");
    a2_2->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    a2_2->set_tag("clojure.lang.BigInt");

    // User AST: arg2 has no unwind-memory on itself

    auto res = engine.compileAST(root, "test2").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();

    bool caught = false;
    try {
      func();
    } catch (const LanguageException &e) {
      caught = true;
    }
    assert_true(caught);
  });
}

// Test Case 3: Exception Unwinding on Third Argument (Nested Throw)
static void test_invoke_exception_unwinding_nested(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine(llvm::OptimizationLevel::O0, false);
    setup_minimal_runtime(engine);

    // (let [thr (fn [] (throw (new java.lang.Exception)))
    //       f (fn [x y z] (+ (+ x y) z))]
    //  (f 10N 20N (f 30N 40N (thr))))

    // 1. Build `thr` fn
    Node thrFn;
    thrFn.set_op(opFn);
    auto *tFn = thrFn.mutable_subnode()->mutable_fn();
    auto *tMn = tFn->add_methods()->mutable_subnode()->mutable_fnmethod();
    tMn->set_fixedarity(0);
    auto *tBody = tMn->mutable_body();
    tBody->set_op(opThrow);
    auto *ex = tBody->mutable_subnode()->mutable_throw_()->mutable_exception();
    ex->set_op(opNew);
    auto *nn = ex->mutable_subnode()->mutable_new_();
    nn->mutable_class_()->set_op(opConst);
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_val(
        "java.lang.Exception");

    // 2. Build `f` fn
    Node fFn;
    fFn.set_op(opFn);
    auto *fFnNode = fFn.mutable_subnode()->mutable_fn();
    auto *fMn = fFnNode->add_methods()->mutable_subnode()->mutable_fnmethod();
    fMn->set_fixedarity(3);
    fMn->add_params()->mutable_subnode()->mutable_binding()->set_name("x");
    fMn->add_params()->mutable_subnode()->mutable_binding()->set_name("y");
    fMn->add_params()->mutable_subnode()->mutable_binding()->set_name("z");

    auto *fBody = fMn->mutable_body();
    fBody->set_op(opStaticCall);
    auto *scOuter = fBody->mutable_subnode()->mutable_staticcall();
    scOuter->set_class_("clojure.lang.Numbers");
    scOuter->set_method("add");

    // Inner (+ x y)
    auto *innerAdd = scOuter->add_args();
    innerAdd->set_op(opStaticCall);
    auto *scInner = innerAdd->mutable_subnode()->mutable_staticcall();
    scInner->set_class_("clojure.lang.Numbers");
    scInner->set_method("add");
    
    auto *argX = scInner->add_args();
    argX->set_op(opLocal);
    argX->mutable_subnode()->mutable_local()->set_name("x");
    argX->mutable_subnode()->mutable_local()->set_local(localTypeArg);
    auto *argY = scInner->add_args();
    argY->set_op(opLocal);
    argY->mutable_subnode()->mutable_local()->set_name("y");
    argY->mutable_subnode()->mutable_local()->set_local(localTypeArg);

    auto *argZ = scOuter->add_args();
    argZ->set_op(opLocal);
    argZ->mutable_subnode()->mutable_local()->set_name("z");
    argZ->mutable_subnode()->mutable_local()->set_local(localTypeArg);

    auto *dmX = fBody->add_dropmemory();
    dmX->set_variablename("x");
    dmX->set_requiredrefcountchange(-1);
    auto *dmY = fBody->add_dropmemory();
    dmY->set_variablename("y");
    dmY->set_requiredrefcountchange(-1);
    auto *dmZ = fBody->add_dropmemory();
    dmZ->set_variablename("z");
    dmZ->set_requiredrefcountchange(-1);

    // 3. Build Root Let
    Node root;
    root.set_op(opLet);
    auto *let = root.mutable_subnode()->mutable_let();
    
    // Binding `thr`
    auto *bThr = let->add_bindings()->mutable_subnode()->mutable_binding();
    bThr->set_name("thr");
    bThr->mutable_init()->CopyFrom(thrFn);

    // Binding `f`
    auto *bF = let->add_bindings()->mutable_subnode()->mutable_binding();
    bF->set_name("f");
    bF->mutable_init()->CopyFrom(fFn);

    // 4. Build invocation: (f 10N 20N (f 30N 40N (thr)))
    auto *invOuter = let->mutable_body();
    invOuter->set_op(opInvoke);
    auto *iOuter = invOuter->mutable_subnode()->mutable_invoke();
    
    // fn `f`
    iOuter->mutable_fn()->set_op(opLocal);
    iOuter->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
    iOuter->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
    auto *uFnOuter = iOuter->mutable_fn()->add_unwindmemory();
    uFnOuter->set_variablename("f");
    uFnOuter->set_requiredrefcountchange(-1);
    auto *uThrOuter = iOuter->mutable_fn()->add_unwindmemory();
    uThrOuter->set_variablename("thr");
    uThrOuter->set_requiredrefcountchange(-1);
    
    // Arg 1: 10N
    auto *oArg1 = iOuter->add_args();
    oArg1->set_op(opConst);
    oArg1->mutable_subnode()->mutable_const_()->set_val("10");
    oArg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    oArg1->set_tag("clojure.lang.BigInt");
    auto *uOArg1_f = oArg1->add_unwindmemory();
    uOArg1_f->set_variablename("f");
    uOArg1_f->set_requiredrefcountchange(-1);
    auto *uOArg1_thr = oArg1->add_unwindmemory();
    uOArg1_thr->set_variablename("thr");
    uOArg1_thr->set_requiredrefcountchange(-1);

    // Arg 2: 20N
    auto *oArg2 = iOuter->add_args();
    oArg2->set_op(opConst);
    oArg2->mutable_subnode()->mutable_const_()->set_val("20");
    oArg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    oArg2->set_tag("clojure.lang.BigInt");
    auto *uOArg2_f = oArg2->add_unwindmemory();
    uOArg2_f->set_variablename("f");
    uOArg2_f->set_requiredrefcountchange(-1);
    auto *uOArg2_thr = oArg2->add_unwindmemory();
    uOArg2_thr->set_variablename("thr");
    uOArg2_thr->set_requiredrefcountchange(-1);

    // Arg 3: (f 30N 40N (thr))
    auto *oArg3 = iOuter->add_args();
    oArg3->set_op(opInvoke);
    auto *iInner = oArg3->mutable_subnode()->mutable_invoke();

    // Inner fn `f`
    iInner->mutable_fn()->set_op(opLocal);
    iInner->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
    iInner->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
    auto *uFnInner_f = iInner->mutable_fn()->add_unwindmemory();
    uFnInner_f->set_variablename("f");
    uFnInner_f->set_requiredrefcountchange(-1);
    auto *uFnInner_thr = iInner->mutable_fn()->add_unwindmemory();
    uFnInner_thr->set_variablename("thr");
    uFnInner_thr->set_requiredrefcountchange(-1);
    auto *dFnInnerF = iInner->mutable_fn()->add_dropmemory();
    dFnInnerF->set_variablename("f");
    dFnInnerF->set_requiredrefcountchange(1);

    // Inner Arg 1: 30N
    auto *iArg1 = iInner->add_args();
    iArg1->set_op(opConst);
    iArg1->mutable_subnode()->mutable_const_()->set_val("30");
    iArg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    iArg1->set_tag("clojure.lang.BigInt");
    auto *uIArg1_f = iArg1->add_unwindmemory();
    uIArg1_f->set_variablename("f");
    uIArg1_f->set_requiredrefcountchange(-1);
    auto *uIArg1_thr = iArg1->add_unwindmemory();
    uIArg1_thr->set_variablename("thr");
    uIArg1_thr->set_requiredrefcountchange(-1);

    // Inner Arg 2: 40N
    auto *iArg2 = iInner->add_args();
    iArg2->set_op(opConst);
    iArg2->mutable_subnode()->mutable_const_()->set_val("40");
    iArg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    iArg2->set_tag("clojure.lang.BigInt");
    auto *uIArg2_f = iArg2->add_unwindmemory();
    uIArg2_f->set_variablename("f");
    uIArg2_f->set_requiredrefcountchange(-1);
    auto *uIArg2_thr = iArg2->add_unwindmemory();
    uIArg2_thr->set_variablename("thr");
    uIArg2_thr->set_requiredrefcountchange(-1);

    // Inner Arg 3: (thr)
    auto *iArg3 = iInner->add_args();
    iArg3->set_op(opInvoke);
    auto *iThr = iArg3->mutable_subnode()->mutable_invoke();
    iThr->mutable_fn()->set_op(opLocal);
    iThr->mutable_fn()->mutable_subnode()->mutable_local()->set_name("thr");
    iThr->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);
    // Unwind for thr itself
    auto *uThrInner_f = iThr->mutable_fn()->add_unwindmemory();
    uThrInner_f->set_variablename("f");
    uThrInner_f->set_requiredrefcountchange(-1);
    auto *uThrInner_thr = iThr->mutable_fn()->add_unwindmemory();
    uThrInner_thr->set_variablename("thr");
    uThrInner_thr->set_requiredrefcountchange(-1);

    auto res = engine.compileAST(root, "test3").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();

    bool caught = false;
    try {
      func();
    } catch (const LanguageException &e) {
      caught = true;
    }
    assert_true(caught);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_invoke_captures_memory),
      cmocka_unit_test(test_invoke_exception_unwinding),
      cmocka_unit_test(test_invoke_exception_unwinding_nested),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
