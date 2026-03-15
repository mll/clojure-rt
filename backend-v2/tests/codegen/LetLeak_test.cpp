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

static void test_let_uaf_fixed(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

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
  bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  bxc->mutable_init()->set_tag("clojure.lang.BigInt");

  auto *body = let->mutable_body();
  body->set_op(opStaticCall);
  auto *sc = body->mutable_subnode()->mutable_staticcall();
  sc->set_class_("rt.Core");
  sc->set_method("Numbers_add");
  
  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  arg1->mutable_subnode()->mutable_const_()->set_val("aa");
  arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);

  auto *arg2 = sc->add_args();
  arg2->set_op(opLocal);
  arg2->mutable_subnode()->mutable_local()->set_name("x");
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

  cout << "=== Let UAF Fixed Test (Should throw LanguageException but NOT crash) ===" << endl;
  uword_t initialCount = atomic_load(&objectCount[bigIntegerType - 1]);
  try {
    {
      rt::JITEngine engine(compState);
      auto res = engine.compileAST(root, "__test_let_uaf", llvm::OptimizationLevel::O0, true).get();
      res.toPtr<RTValue (*)()>()();
      assert_true(false); // Should have thrown
    }
  } catch (const rt::LanguageException &e) {
    cout << "Caught expected LanguageException: " << rt::getExceptionString(e) << endl;
  }
  uword_t finalCount = atomic_load(&objectCount[bigIntegerType - 1]);
  cout << "BigInt initial: " << initialCount << ", final: " << finalCount << endl;
  // In the fixed case, there should be no additional BigInts left.
  assert_int_equal(finalCount, initialCount);
}

static void test_let_gap_leak(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // (let [x 1N] (do (+ "aa" "bb") x))
  // The first Numbers_add throws. x is never consumed. x is NOT protected by let anymore.
  // This SHOULD leak.
  Node root;
  root.set_op(opLet);
  auto *let = root.mutable_subnode()->mutable_let();

  auto *bx = let->add_bindings();
  bx->set_op(opBinding);
  auto *bxc = bx->mutable_subnode()->mutable_binding();
  bxc->set_name("x");
  bxc->mutable_init()->set_op(opConst);
  bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_val("1");
  bxc->mutable_init()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
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
  a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
  auto *a2 = sc->add_args();
  a2->set_op(opConst);
  a2->mutable_subnode()->mutable_const_()->set_val("bb");
  a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);

  auto *ret = do_->mutable_ret();
  ret->set_op(opLocal);
  ret->mutable_subnode()->mutable_local()->set_name("x");
  ret->mutable_subnode()->mutable_local()->set_local(localTypeLet);

  // Add unwind-memory guidance to simulate real compiler output
  // If Numbers_add throws, x must be released.
  auto* g1 = body->add_unwindmemory();
  g1->set_variablename("x");
  g1->set_requiredrefcountchange(-1);

  auto* g2 = stmt->add_unwindmemory();
  g2->set_variablename("x");
  g2->set_requiredrefcountchange(-1);

  auto* g3 = ret->add_unwindmemory();
  g3->set_variablename("x");
  g3->set_requiredrefcountchange(-1);

  auto* g4 = a1->add_unwindmemory();
  g4->set_variablename("x");
  g4->set_requiredrefcountchange(-1);

  auto* g5 = a2->add_unwindmemory();
  g5->set_variablename("x");
  g5->set_requiredrefcountchange(-1);

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

  cout << "=== Let Gap Leak Test (Should leak 1N) ===" << endl;
  uword_t initialCount = atomic_load(&objectCount[bigIntegerType - 1]);
  try {
    {
      rt::JITEngine engine(compState);
      auto res = engine.compileAST(root, "__test_let_leak", llvm::OptimizationLevel::O0, true).get();
      res.toPtr<RTValue (*)()>()();
      assert_true(false); // Should have thrown
    }
  } catch (const rt::LanguageException &e) {
    cout << "Caught expected LanguageException: " << rt::getExceptionString(e) << endl;
  }
  uword_t finalCount = atomic_load(&objectCount[bigIntegerType - 1]);
  cout << "BigInt initial: " << initialCount << ", final: " << finalCount << endl;
  // This should now NOT leak because unwind-memory handles the release during exceptions.
  assert_int_equal(finalCount, initialCount);
}

static void test_do_statement_leak(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // (do (+ 1N 2N) 42)
  // The result of (+ 1N 2N) is a BigInt statement that must be released.
  Node root;
  root.set_op(opDo);
  auto *do_ = root.mutable_subnode()->mutable_do_();

  auto *stmt = do_->add_statements();
  stmt->set_op(opStaticCall);
  auto *sc = stmt->mutable_subnode()->mutable_staticcall();
  sc->set_class_("clojure.lang.Numbers");
  sc->set_method("add");
  
  auto *a1 = sc->add_args();
  a1->set_op(opConst);
  a1->mutable_subnode()->mutable_const_()->set_val("1");
  a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  a1->set_tag("clojure.lang.BigInt");

  auto *a2 = sc->add_args();
  a2->set_op(opConst);
  a2->mutable_subnode()->mutable_const_()->set_val("2");
  a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  a2->set_tag("clojure.lang.BigInt");

  auto *ret = do_->mutable_ret();
  ret->set_op(opConst);
  ret->mutable_subnode()->mutable_const_()->set_val("42");
  ret->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  ret->set_tag("long");

  // Setup clojure.lang.Numbers/add metadata
  auto desc = std::make_unique<rt::ClassDescription>();
  rt::IntrinsicDescription numAdd;
  numAdd.type = rt::CallType::Call;
  numAdd.symbol = "mock_add_bigint"; 
  numAdd.argTypes = {rt::ObjectTypeSet::all(), rt::ObjectTypeSet::all()};
  numAdd.returnType = rt::ObjectTypeSet::all();
  desc->staticFns["add"].push_back(numAdd);

  String *numName = String_createDynamicStr("clojure.lang.Numbers");
  Ptr_retain(numName);
  ::Class *numCls = Class_create(numName, numName, 0, nullptr);
  numCls->compilerExtension = desc.release();
  numCls->compilerExtensionDestructor = rt::delete_class_description;
  compState.classRegistry.registerObject("clojure.lang.Numbers", numCls);

  cout << "=== Do Statement Leak Test (Should NOT leak) ===" << endl;
  uword_t initialCount = atomic_load(&objectCount[bigIntegerType - 1]);
  {
    rt::JITEngine engine(compState);
    auto res = engine.compileAST(root, "__test_do_leak", llvm::OptimizationLevel::O0, true).get();
    
    // Account for constants created during compilation
    uword_t afterCompileCount = atomic_load(&objectCount[bigIntegerType - 1]);
    cout << "BigInt initial: " << initialCount << ", after compile: " << afterCompileCount << endl;

    RTValue val = res.toPtr<RTValue (*)()>()();
    assert_int_equal(42, RT_unboxInt32(val));
    
    uword_t finalCount = atomic_load(&objectCount[bigIntegerType - 1]);
    cout << "BigInt final: " << finalCount << endl;
    
    // The statement result should have been released. 
    // Any BigInts created in the mock should be gone.
    assert_int_equal(finalCount, afterCompileCount);
  }
}

static void test_if_test_leak(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  // (if (+ 1N 2N) 42 43)
  // The result of (+ 1N 2N) is used as a test condition and must be released.
  Node root;
  root.set_op(opIf);
  auto *if_ = root.mutable_subnode()->mutable_if_();

  auto *testNode = if_->mutable_test();
  testNode->set_op(opStaticCall);
  auto *sc = testNode->mutable_subnode()->mutable_staticcall();
  sc->set_class_("clojure.lang.Numbers");
  sc->set_method("add");
  
  auto *a1 = sc->add_args();
  a1->set_op(opConst);
  a1->mutable_subnode()->mutable_const_()->set_val("1");
  a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  a1->set_tag("clojure.lang.BigInt");

  auto *a2 = sc->add_args();
  a2->set_op(opConst);
  a2->mutable_subnode()->mutable_const_()->set_val("2");
  a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  a2->set_tag("clojure.lang.BigInt");

  auto *then = if_->mutable_then();
  then->set_op(opConst);
  then->mutable_subnode()->mutable_const_()->set_val("42");
  then->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  then->set_tag("long");

  auto *else_ = if_->mutable_else_();
  else_->set_op(opConst);
  else_->mutable_subnode()->mutable_const_()->set_val("43");
  else_->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  else_->set_tag("long");

  // Setup clojure.lang.Numbers/add metadata
  auto desc = std::make_unique<rt::ClassDescription>();
  rt::IntrinsicDescription numAdd;
  numAdd.type = rt::CallType::Call;
  numAdd.symbol = "mock_add_bigint"; 
  numAdd.argTypes = {rt::ObjectTypeSet::all(), rt::ObjectTypeSet::all()};
  numAdd.returnType = rt::ObjectTypeSet::all();
  desc->staticFns["add"].push_back(numAdd);

  String *numName = String_createDynamicStr("clojure.lang.Numbers");
  Ptr_retain(numName);
  ::Class *numCls = Class_create(numName, numName, 0, nullptr);
  numCls->compilerExtension = desc.release();
  numCls->compilerExtensionDestructor = rt::delete_class_description;
  compState.classRegistry.registerObject("clojure.lang.Numbers", numCls);

  cout << "=== If Test Leak Test (Should NOT leak) ===" << endl;
  uword_t initialCount = atomic_load(&objectCount[bigIntegerType - 1]);
  (void)initialCount; // Silence warning
  {
    rt::JITEngine engine(compState);
    auto res = engine.compileAST(root, "__test_if_leak", llvm::OptimizationLevel::O0, true).get();
    
    uword_t afterCompileCount = atomic_load(&objectCount[bigIntegerType - 1]);
    RTValue val = res.toPtr<RTValue (*)()>()();
    assert_int_equal(42, RT_unboxInt32(val));
    
    uword_t finalCount = atomic_load(&objectCount[bigIntegerType - 1]);
    cout << "BigInt after compile: " << afterCompileCount << ", final: " << finalCount << endl;
    assert_int_equal(finalCount, afterCompileCount);
  }
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_let_uaf_fixed),
      cmocka_unit_test(test_let_gap_leak),
      cmocka_unit_test(test_do_statement_leak),
      cmocka_unit_test(test_if_test_leak),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
