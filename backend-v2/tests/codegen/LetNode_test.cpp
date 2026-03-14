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

static void test_let_basic(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

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
  bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);

  // Binding y = 2
  auto *by = let->add_bindings();
  by->set_op(opBinding);
  auto *byc = by->mutable_subnode()->mutable_binding();
  byc->set_name("y");
  byc->mutable_init()->set_op(opConst);
  byc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("2");
  byc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);

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
  numAdd.argTypes = {rt::ObjectTypeSet::all(), rt::ObjectTypeSet::all()};
  numAdd.returnType = rt::ObjectTypeSet::all();
  desc->staticFns["Numbers_add"].push_back(numAdd);

  String *coreName = String_createDynamicStr("rt.Core");
  Ptr_retain(coreName);
  ::Class *coreCls = Class_create(coreName, coreName, 0, nullptr);
  coreCls->compilerExtension = desc.release();
  coreCls->compilerExtensionDestructor = rt::delete_class_description;
  compState.classRegistry.registerObject("rt.Core", coreCls);

  try {
    auto res = engine.compileAST(root, "__test_let_basic", llvm::OptimizationLevel::O0, true).get();
    RTValue result = res.toPtr<RTValue (*)()>()();
    assert_int_equal(RT_unboxInt32(result), 3);
  } catch (const rt::LanguageException &e) {
    cout << "Caught LanguageException: " << rt::getExceptionString(e) << endl;
    assert_true(false);
  } catch (const std::exception &e) {
    cout << "Caught exception: " << e.what() << endl;
    assert_true(false);
  }
}

static void test_let_memory_mm(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

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
  bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);

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
    auto res = engine.compileAST(root, "__test_let_mm", llvm::OptimizationLevel::O0, true).get();
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
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_let_basic),
      cmocka_unit_test(test_let_memory_mm),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
