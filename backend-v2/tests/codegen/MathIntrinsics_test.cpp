#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include "runtime/Keyword.h"
#include <cmath>
#include <iostream>

extern "C" {
#include "../../runtime/BigInteger.h"
#include "../../runtime/String.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>

void delete_class_description(void *ptr);
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_math_runtime_test(rt::ThreadsafeCompilerState &compState) {
  // BigInt class
  {
    String *nameStr = String_create("clojure.lang.BigInt");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    compState.classRegistry.registerObject("clojure.lang.BigInt", cls);
  }

  // java.lang.Math class
  {
    String *nameStr = String_create("java.lang.Math");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

    ClassDescription *ext = new ClassDescription();
    ext->name = "java.lang.Math";

    auto addUnary = [&](const string &method, const string &symbol) {
      // double
      IntrinsicDescription idD;
      idD.symbol = symbol;
      idD.type = CallType::Intrinsic;
      idD.returnType = ObjectTypeSet(doubleType, false);
      idD.argTypes = {ObjectTypeSet(doubleType, false)};
      ext->staticFns[method].push_back(idD);

      // int
      IntrinsicDescription idI = idD;
      idI.argTypes = {ObjectTypeSet(integerType, false)};
      ext->staticFns[method].push_back(idI);

      // bigint
      IntrinsicDescription idB = idD;
      idB.argTypes = {ObjectTypeSet(bigIntegerType, false)};
      ext->staticFns[method].push_back(idB);
    };

    auto addBinary = [&](const string &method, const string &symbol) {
      // double, double
      IntrinsicDescription idD;
      idD.symbol = symbol;
      idD.type = CallType::Intrinsic;
      idD.returnType = ObjectTypeSet(doubleType, false);
      idD.argTypes = {ObjectTypeSet(doubleType, false), ObjectTypeSet(doubleType, false)};
      ext->staticFns[method].push_back(idD);

      // int, int
      IntrinsicDescription idI = idD;
      idI.argTypes = {ObjectTypeSet(integerType, false), ObjectTypeSet(integerType, false)};
      ext->staticFns[method].push_back(idI);

      // any, any
      IntrinsicDescription idA = idD;
      idA.argTypes = {ObjectTypeSet::all(), ObjectTypeSet::all()};
      ext->staticFns[method].push_back(idA);
    };

    addUnary("sin", "Math_sin");
    addUnary("sqrt", "Math_sqrt");
    addBinary("pow", "Math_pow");

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject("java.lang.Math", cls);
  }
}

static void test_math_sin_int(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  setup_math_runtime_test(compState);
  JITEngine engine(compState);

  // (Math/sin 0)
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("java.lang.Math");
  sc->set_method("sin");

  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  auto *c1 = arg1->mutable_subnode()->mutable_const_();
  c1->set_val("0");
  c1->set_type(ConstNode_ConstType_constTypeNumber);
  arg1->set_tag("long");

  try {
    auto resCall = engine
                       .compileAST(callNode, "__test_sin_int",
                                   llvm::OptimizationLevel::O0, false)
                       .get();
    RTValue result = resPtrToValue(resCall);
    assert_true(RT_isDouble(result));
    assert_double_equal(0.0, RT_unboxDouble(result), 0.0001);
    release(result);
  } catch (const std::exception &e) {
    fprintf(stderr, "Math sin int failure: %s\n", e.what());
    assert_true(false);
  }
}

static void test_math_sqrt_bigint(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  setup_math_runtime_test(compState);
  JITEngine engine(compState);

  // (Math/sqrt 100N)
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("java.lang.Math");
  sc->set_method("sqrt");

  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  auto *c1 = arg1->mutable_subnode()->mutable_const_();
  c1->set_val("100");
  c1->set_type(ConstNode_ConstType_constTypeNumber);
  arg1->set_tag("clojure.lang.BigInt");

  try {
    auto resCall = engine
                       .compileAST(callNode, "__test_sqrt_bigint",
                                   llvm::OptimizationLevel::O0, false)
                       .get();
    // Protect constant
    for (auto val : engine.getModuleConstants("__test_sqrt_bigint"))
      retain(val);

    RTValue result = resPtrToValue(resCall);
    assert_true(RT_isDouble(result));
    assert_double_equal(10.0, RT_unboxDouble(result), 0.0001);
    release(result);
  } catch (const std::exception &e) {
    fprintf(stderr, "Math sqrt bigint failure: %s\n", e.what());
    assert_true(false);
  }
}

static void test_math_sqrt_leak_repro(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  setup_math_runtime_test(compState);
  JITEngine engine(compState);

  // Create a String 'a' and bind it to a Var
  {
    RTValue k = Keyword_create(String_create("a"));
    Var *v = Var_create(k);
    RTValue str = RT_boxPtr(String_create("aa"));

    Ptr_retain(v); // Var_bindRoot consumes v, so retain it
    Var_bindRoot(v, str);

    compState.varRegistry.registerObject("a", v);
  }

  // (Math/sqrt a)
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("java.lang.Math");
  sc->set_method("sqrt");

  auto *arg1 = sc->add_args();
  arg1->set_op(opVar);
  auto *v1 = arg1->mutable_subnode()->mutable_var();
  v1->set_var("v_a");

  try {
    auto resCall = engine
                       .compileAST(callNode, "__test_leak_repro",
                                   llvm::OptimizationLevel::O0, true)
                       .get();
    fflush(stdout);
    fflush(stderr);
    RTValue result = resPtrToValue(resCall); // Should throw
    (void)result;
  } catch (const std::exception &e) {
    printf("Caught expected exception: %s\n", e.what());
  } catch (...) {
    printf("Caught UNKNOWN exception!\n");
  }

  // Note: initialise_memory() and RuntimeInterface_cleanup() in main will check
  // for leaks. If it leaks, RuntimeInterface_cleanup() will print the leak
  // detection and fail (usually).
}

static void test_math_pow_var(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  setup_math_runtime_test(compState);
  JITEngine engine(compState);

  // (def a 2)
  {
    RTValue v_a = RT_boxInt32(2);
    RTValue k = Keyword_create(String_create("a"));
    Var *v = Var_create(k);
    
    Ptr_retain(v);
    Var_bindRoot(v, v_a);
    compState.varRegistry.registerObject("a", v);
  }

  // (Math/pow 2.0 a)
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("java.lang.Math");
  sc->set_method("pow");

  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  auto *c1 = arg1->mutable_subnode()->mutable_const_();
  c1->set_val("2.0");
  c1->set_type(ConstNode_ConstType_constTypeNumber);
  arg1->set_tag("double");

  auto *arg2 = sc->add_args();
  arg2->set_op(opVar);
  auto *v2 = arg2->mutable_subnode()->mutable_var();
  v2->set_var("v_a");

  try {
    auto resCall = engine
                       .compileAST(callNode, "__test_pow_var",
                                   llvm::OptimizationLevel::O0, false)
                       .get();
    RTValue result = resPtrToValue(resCall);
    assert_true(RT_isDouble(result));
    assert_double_equal(4.0, RT_unboxDouble(result), 0.0001);
    release(result);
  } catch (const std::exception &e) {
    fprintf(stderr, "Math pow var failure: %s\n", e.what());
    assert_true(false);
  }
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_math_sin_int),
      cmocka_unit_test(test_math_sqrt_bigint),
      cmocka_unit_test(test_math_sqrt_leak_repro),
      cmocka_unit_test(test_math_pow_var),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
