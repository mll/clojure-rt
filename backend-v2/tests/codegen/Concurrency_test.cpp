#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Numbers.h"
#include "../../runtime/Object.h"
#include "../../runtime/String.h"
#include "../../runtime/Var.h"
#include "../../tools/EdnParser.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>

void delete_class_description(void *ptr);
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void setup_minimal_runtime(JITEngine &engine) {
  auto &compState = engine.threadsafeState;
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

static void test_concurrent_closures(void **state) {
  (void)state;
  // We increase the chance of catching race conditions by running many
  // iterations. However, for standard unit tests, we keep it reasonable.

  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    setup_minimal_runtime(engine);

    // 1. Create the closure with a capture
    // (let [x 100N] (fn [y] (+ x y)))
    Node fnDef;
    fnDef.set_op(opLet);
    auto *let = fnDef.mutable_subnode()->mutable_let();
    auto *bx = let->add_bindings();
    bx->set_op(opBinding);
    bx->mutable_subnode()->mutable_binding()->set_name("x");
    bx->mutable_subnode()->mutable_binding()->mutable_init()->set_op(opConst);
    bx->mutable_subnode()
        ->mutable_binding()
        ->mutable_init()
        ->mutable_subnode()
        ->mutable_const_()
        ->set_val("100");
    bx->mutable_subnode()
        ->mutable_binding()
        ->mutable_init()
        ->mutable_subnode()
        ->mutable_const_()
        ->set_type(ConstNode_ConstType_constTypeNumber);
    bx->mutable_subnode()->mutable_binding()->mutable_init()->set_tag(
        "clojure.lang.BigInt");

    auto *fn = let->mutable_body();
    fn->set_op(opFn);
    auto *f = fn->mutable_subnode()->mutable_fn();
    auto *mn = f->add_methods()->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(1);
    mn->add_params()->mutable_subnode()->mutable_binding()->set_name("y");
    mn->add_closedovers()->set_op(opLocal);
    mn->mutable_closedovers(0)->mutable_subnode()->mutable_local()->set_name(
        "x");
    mn->mutable_closedovers(0)->mutable_subnode()->mutable_local()->set_local(
        localTypeLet);

    // Guidance to align with frontend IR EXACTLY:
    // 1. On fn node: release the let-binding reference on exception path (Match EDN: :unwind-memory [["x__#0" -1]])
    auto *gUnwind = fn->add_unwindmemory();
    gUnwind->set_variablename("x");
    gUnwind->set_requiredrefcountchange(-1);

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

    // Guidance to align with frontend IR:
    // On static-call node: retain 'x' before call (since Numbers/add consumes its args)
    auto *gPlus = plus->add_dropmemory();
    gPlus->set_variablename("x");
    gPlus->set_requiredrefcountchange(1);

    auto builderAddr =
        engine.compileAST(fnDef, "closure_builder").get().address;
    RTValue funObj = builderAddr.toPtr<RTValue (*)()>()();

    // Promote the function object and its captures to shared!
    promoteToShared(funObj);

    // Find/Create the Var. Var_create returns with +1 (shared).
    const char *varName = "user/shared-fn";
    Var *v = Var_create(Keyword_create(String_create(varName)));
    Ptr_retain(v); // For Var_bindRoot (consumes 1)
    Var_bindRoot(v, funObj);
    engine.threadsafeState.varRegistry.registerObject(varName, v);

    // 2. Create the caller
    // (fn [] (user/shared-fn 5))
    Node callerDef;
    callerDef.set_op(opInvoke);
    auto *inv = callerDef.mutable_subnode()->mutable_invoke();
    inv->mutable_fn()->set_op(opVar);
    inv->mutable_fn()->mutable_subnode()->mutable_var()->set_var(
        std::string("#'") + varName);
    auto *arg = inv->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_val("5");
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);

    auto callerAddr = engine.compileAST(callerDef, "caller").get().address;
    typedef RTValue (*CallerFn)();
    CallerFn caller = (CallerFn)callerAddr.getValue();

    // 3. Spawning threads
    std::atomic<bool> start{false};
    const int numThreads = 8;
    const int iterations = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
      threads.emplace_back([&]() {
        while (!start.load())
          std::this_thread::yield();
        for (int j = 0; j < iterations; ++j) {
          RTValue res = caller();
          if (getType(res) != bigIntegerType ||
              mpz_get_si(((BigInteger *)RT_unboxPtr(res))->value) != 105) {
            fprintf(stderr, "Incorrect result in thread! Type: %d, Val: %ld\n",
                    getType(res),
                    RT_isPtr(res) ? (long)mpz_get_si(
                                        ((BigInteger *)RT_unboxPtr(res))->value)
                                  : (long)RT_unboxInt32(res));
            abort();
          }
          release(res);
        }
      });
    }

    start.store(true);
    for (auto &t : threads)
      t.join();
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_concurrent_closures),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
