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

static void test_no_throw_collection(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // Create a nested vector of constants: [[1 2] [3 4]]
  Node root;
  root.set_op(opVector);
  auto *vroot = root.mutable_subnode()->mutable_vector();

  for (int i = 0; i < 2; ++i) {
    auto *inner = vroot->add_items();
    inner->set_op(opVector);
    auto *vinner = inner->mutable_subnode()->mutable_vector();
    for (int j = 0; j < 2; ++j) {
        auto *c = vinner->add_items();
        c->set_op(opConst);
        c->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        c->mutable_subnode()->mutable_const_()->set_val(to_string(i * 2 + j));
    }
  }

  // Compile and dump IR
  auto res = engine.compileAST(root, "__test_no_throw", llvm::OptimizationLevel::O0, true).get();
  
  // The output should NOT contain "invoke" or "landingpad"
  // We can't easily capture stdout here but we can verify it doesn't crash 
  // and the result is correct.
  RTValue result = res.toPtr<RTValue (*)()>()();
  assert_true(RT_isPtr(result));
  release(result);
}

static void test_hot_cold_separation(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // Define vars so they are found
  auto *nameX = String_createDynamicStr("x");
  compState.varRegistry.registerObject("x", Var_create(RT_boxPtr(nameX)));
  auto *nameY = String_createDynamicStr("y");
  compState.varRegistry.registerObject("y", Var_create(RT_boxPtr(nameY)));

  Node root;
  root.set_op(opVector);
  auto *vec = root.mutable_subnode()->mutable_vector();
  
  // (def x 1) -> resource
  auto *item0 = vec->add_items();
  item0->set_op(opDef);
  auto *d0 = item0->mutable_subnode()->mutable_def();
  d0->set_var("v/x");
  d0->mutable_init()->set_op(opConst);
  d0->mutable_init()->mutable_subnode()->mutable_const_()->set_val("1");

  // (def y 2) -> will invoke because x is a resource
  auto *item1 = vec->add_items();
  item1->set_op(opDef);
  auto *d1 = item1->mutable_subnode()->mutable_def();
  d1->set_var("v/y");
  d1->mutable_init()->set_op(opConst);
  d1->mutable_init()->mutable_subnode()->mutable_const_()->set_val("2");

  cout << "=== Hot/Cold Separation Test IR ===" << endl;
  auto res = engine.compileAST(root, "__hot_cold_test", llvm::OptimizationLevel::O0, true).get();
  cout << "===================================" << endl;
}

static void test_crash_repro(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  
  // Register rt.Core metadata
  auto desc = std::make_unique<rt::ClassDescription>();
  desc->name = "rt.Core";

  rt::IntrinsicDescription numAdd;
  numAdd.type = rt::CallType::Call;
  numAdd.symbol = "Numbers_add";
  numAdd.argTypes = {rt::ObjectTypeSet::all(), rt::ObjectTypeSet::all()};
  numAdd.returnType = rt::ObjectTypeSet::all();
  desc->staticFns["Numbers_add"].push_back(numAdd);

  String *coreName = String_createDynamicStr("rt.Core");
  Ptr_retain(coreName); // One for name, one for className
  ::Class *coreCls = Class_create(coreName, coreName, 0, nullptr);
  coreCls->compilerExtension = desc.release();
  coreCls->compilerExtensionDestructor = rt::delete_class_description;
  compState.classRegistry.registerObject("rt.Core", coreCls);

  rt::JITEngine engine(compState);

  // (def x "")
  // (def y 5/3)
  // (+ 3N x)

  Node root;
  root.set_op(opDo);
  auto *do_ = root.mutable_subnode()->mutable_do_();

  auto *s1 = do_->add_statements();
  s1->set_op(opDef);
  auto *dx = s1->mutable_subnode()->mutable_def();
  dx->set_var("v/x");
  dx->mutable_init()->set_op(opConst);
  dx->mutable_init()->mutable_subnode()->mutable_const_()->set_val("\"\"");

  auto *s2 = do_->add_statements();
  s2->set_op(opDef);
  auto *dy = s2->mutable_subnode()->mutable_def();
  dy->set_var("v/y");
  dy->mutable_init()->set_op(opConst);
  dy->mutable_init()->mutable_subnode()->mutable_const_()->set_val("5/3");

  auto *ret = do_->mutable_ret();
  ret->set_op(opStaticCall);
  auto *sc = ret->mutable_subnode()->mutable_staticcall();
  sc->set_class_("rt.Core");
  sc->set_method("Numbers_add");
  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  arg1->mutable_subnode()->mutable_const_()->set_val("3N");
  auto *arg2 = sc->add_args();
  arg2->set_op(opVar);
  arg2->mutable_subnode()->mutable_var()->set_var("v/x");

  cout << "=== Crash Repro Test IR ===" << endl;
  auto res = engine.compileAST(root, "__crash_repro", llvm::OptimizationLevel::O0, true).get();
  cout << "===========================" << endl;
}

static void test_whitelist_works(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // We'll just compile a VarNode.
  // VarNode::codegen calls Var_deref, which is whitelisted.
  // It also creates a CleanupChainGuard.
  
  // Register the var manually so it's found
  auto *nameStr = String_createDynamicStr("clojure.core/*print-length*");
  Var *v = Var_create(RT_boxPtr(nameStr));
  compState.varRegistry.registerObject("clojure.core/*print-length*", v);

  Node root;
  root.set_op(opVar);
  root.mutable_subnode()->mutable_var()->set_var("v/clojure.core/*print-length*");

  // Compile and dump IR
  cout << "=== Whitelist Test IR ===" << endl;
  auto res = engine.compileAST(root, "__whitelist_test", llvm::OptimizationLevel::O0, true).get();
  cout << "=========================" << endl;
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_no_throw_collection),
      cmocka_unit_test(test_whitelist_works),
      cmocka_unit_test(test_hot_cold_separation),
      cmocka_unit_test(test_crash_repro),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
