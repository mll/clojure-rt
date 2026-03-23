#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Mock implementations for dispatch to call
extern "C" {
int32_t mock_add_int(int32_t a, int32_t b) {
  return a + b + 1000; // Offset to verify this was called
}

double mock_add_double(double a, double b) {
  return a + b + 2000.0; // Offset to verify
}

RTValue mock_add_generic(RTValue a, RTValue b) {
  if (RT_isInt32(a) && RT_isInt32(b)) {
    return RT_boxInt32(RT_unboxInt32(a) + RT_unboxInt32(b) + 5000);
  }
  if (RT_isDouble(a) && RT_isDouble(b)) {
    return RT_boxDouble(RT_unboxDouble(a) + RT_unboxDouble(b) + 6000);
  }
  return RT_boxNil();
}

bool mock_get_true() { return true; }
}

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_mock_runtime_full(rt::ThreadsafeCompilerState &compState) {
  // Numbers class with overloaded add: 2 specialized (int, double), 1 generic
  {
    String *nameStr = String_create("clojure.lang.Numbers");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

    ClassDescription *ext = new ClassDescription();
    ext->name = "clojure.lang.Numbers";

    // 1. Specialized Int + Int
    IntrinsicDescription addII;
    addII.symbol = "mock_add_int";
    addII.type = CallType::Call;
    addII.argTypes.push_back(ObjectTypeSet(integerType, false));
    addII.argTypes.push_back(ObjectTypeSet(integerType, false));
    addII.returnType = ObjectTypeSet(integerType, false);
    ext->staticFns["add"].push_back(addII);

    // 2. Specialized Double + Double
    IntrinsicDescription addDD;
    addDD.symbol = "mock_add_double";
    addDD.type = CallType::Call;
    addDD.argTypes.push_back(ObjectTypeSet(doubleType, false));
    addDD.argTypes.push_back(ObjectTypeSet(doubleType, false));
    addDD.returnType = ObjectTypeSet(doubleType, false);
    ext->staticFns["add"].push_back(addDD);

    // 3. Generic Object + Object
    IntrinsicDescription addGen;
    addGen.symbol = "mock_add_generic";
    addGen.type = CallType::Call;
    addGen.argTypes.push_back(ObjectTypeSet::dynamicType());
    addGen.argTypes.push_back(ObjectTypeSet::dynamicType());
    addGen.returnType = ObjectTypeSet::dynamicType();
    ext->staticFns["add"].push_back(addGen);

    // --- Exhaustive Method (No Generic) ---
    // 1. int int
    ext->staticFns["ex_add"].push_back(addII);
    // 2. double double
    ext->staticFns["ex_add"].push_back(addDD);
    // 3. bool bool (dummy third one)
    IntrinsicDescription addBB;
    addBB.symbol = "mock_get_true"; // just a dummy
    addBB.type = CallType::Call;
    addBB.argTypes.push_back(ObjectTypeSet(booleanType, false));
    addBB.argTypes.push_back(ObjectTypeSet(booleanType, false));
    addBB.returnType = ObjectTypeSet(booleanType, false);
    ext->staticFns["ex_add"].push_back(addBB);

    // --- Complex Method for PHI Node Segfault Regression ---
    // This uses the REAL "Add" intrinsic which has internal branching
    IntrinsicDescription addComplex;
    addComplex.symbol = "Add";
    addComplex.type = CallType::Intrinsic;
    addComplex.argTypes.push_back(ObjectTypeSet(integerType, false));
    addComplex.argTypes.push_back(ObjectTypeSet(integerType, false));
    addComplex.returnType = ObjectTypeSet(integerType, false);
    ext->staticFns["complex_add"].push_back(addComplex);

    // Add a generic fallback so it's a 3-tailed (or multi-tailed) dispatch
    ext->staticFns["complex_add"].push_back(addGen);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
  }

  // Helper class
  {
    String *helperName = String_create("Helper");
    Ptr_retain(helperName);
    Class *hcls = Class_create(helperName, helperName, 0, nullptr);
    ClassDescription *hext = new ClassDescription();
    hext->name = "Helper";
    IntrinsicDescription getTrue;
    getTrue.symbol = "mock_get_true";
    getTrue.type = CallType::Call;
    getTrue.argTypes = {};
    getTrue.returnType = ObjectTypeSet(booleanType, false);
    hext->staticFns["getTrue"].push_back(getTrue);
    hcls->compilerExtension = hext;
    hcls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject("Helper", hcls);
  }
}

static void createIndeterminateArg(Node *node, const char *val, bool isDouble) {
  node->set_op(opIf);
  auto *if_ = node->mutable_subnode()->mutable_if_();
  auto *test = if_->mutable_test();
  test->set_op(opStaticCall);
  test->mutable_subnode()->mutable_staticcall()->set_class_("Helper");
  test->mutable_subnode()->mutable_staticcall()->set_method("getTrue");

  auto *then = if_->mutable_then();
  then->set_op(opConst);
  then->mutable_subnode()->mutable_const_()->set_type(
      ConstNode_ConstType_constTypeNumber);
  then->mutable_subnode()->mutable_const_()->set_val(val);
  then->set_tag(isDouble ? "double" : "long");

  auto *else_ = if_->mutable_else_();
  else_->set_op(opConst);
  else_->mutable_subnode()->mutable_const_()->set_type(
      ConstNode_ConstType_constTypeString);
  else_->mutable_subnode()->mutable_const_()->set_val("dummy");
}

static void test_dynamic_dispatch_3tail(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);

    // Test Case 1: Int + Int
    {
      Node callNode;
      callNode.set_op(opStaticCall);
      auto *sc = callNode.mutable_subnode()->mutable_staticcall();
      sc->set_class_("clojure.lang.Numbers");
      sc->set_method("add");
      createIndeterminateArg(sc->add_args(), "10", false);
      createIndeterminateArg(sc->add_args(), "20", false);

      [[maybe_unused]] auto resCall =
          engine
              .compileAST(callNode, "__test_3tail_int",
                          llvm::OptimizationLevel::O0, false)
              .get()
              .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(1030, RT_unboxInt32(result));
      release(result);
    }

    // Test Case 2: Double + Double
    {
      Node callNode;
      callNode.set_op(opStaticCall);
      auto *sc = callNode.mutable_subnode()->mutable_staticcall();
      sc->set_class_("clojure.lang.Numbers");
      sc->set_method("add");
      createIndeterminateArg(sc->add_args(), "10.5", true);
      createIndeterminateArg(sc->add_args(), "20.5", true);

      auto resCallD = engine
                          .compileAST(callNode, "__test_3tail_double",
                                      llvm::OptimizationLevel::O0, false)
                          .get()
                          .address;
      RTValue resultD = resPtrToValue(resCallD);
      assert_true(RT_isDouble(resultD));
      assert_double_equal(2031.0, RT_unboxDouble(resultD), 0.001);
      release(resultD);
    }
  });
}

static void test_dynamic_dispatch_filtering(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    // Arg 1: Statically Double
    {
      auto *arg1 = sc->add_args();
      arg1->set_op(opConst);
      arg1->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeNumber);
      arg1->mutable_subnode()->mutable_const_()->set_val("10.5");
      arg1->set_tag("double");
    }

    // Arg 2: Statically Any (using If)
    createIndeterminateArg(sc->add_args(), "20.5", true);

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_filtering",
                                     llvm::OptimizationLevel::O0, false)
                         .get()
                         .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isDouble(result));
      assert_double_equal(10.5 + 20.5 + 2000.0, RT_unboxDouble(result), 0.001);
      release(result);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_dynamic_dispatch_exhaustive(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    // Call "ex_add" (no generic, 3 specializations)
    // Args: statically Any (using If)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("ex_add");

    createIndeterminateArg(sc->add_args(), "10", false);
    createIndeterminateArg(sc->add_args(), "20", false);

    try {
      [[maybe_unused]] auto resCall =
          engine
              .compileAST(callNode, "__test_exhaustive",
                          llvm::OptimizationLevel::O0, false)
              .get()
              .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(1030, RT_unboxInt32(result));
      release(result);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_dynamic_dispatch_no_match(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    // Call "ex_add" but with (String, String) -> Should throw runtime exception
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("ex_add");

    // Arg 1: String
    {
      auto *arg1 = sc->add_args();
      arg1->set_op(opConst);
      arg1->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeString);
      arg1->mutable_subnode()->mutable_const_()->set_val("hello");
    }
    // Arg 2: String
    {
      auto *arg2 = sc->add_args();
      arg2->set_op(opConst);
      arg2->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeString);
      arg2->mutable_subnode()->mutable_const_()->set_val("world");
    }

    try {
      [[maybe_unused]] auto resCall =
          engine
              .compileAST(callNode, "__test_no_match",
                          llvm::OptimizationLevel::O0, false)
              .get()
              .address;
      // Since both are strings, it's statically possible they match a dynamic
      // overload if it existed, but we filtered versions. (String, String) is
      // NOT statically possible for any of (int, int), (double, double), (bool,
      // bool). Wait, if it's NOT statically possible, codegen should throw
      // compile-time error!
      assert_true(false); // Should not reach here
    } catch (const LanguageException &e) {
      // Correctly threw compile-time exception because no statically possible
      // overloads exist
      assert_true(true);
    } catch (const std::exception &e) {
      fprintf(stderr, "Unexpected exception: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_dynamic_dispatch_no_match_runtime(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    // To test RUNTIME exception, we need arguments that are statically ANY but
    // runtime NOT matching any specialization.
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("ex_add");

    // Helper method to create a truly generic "String" at runtime
    auto createGenericString = [&](Node *node) {
      node->set_op(opIf);
      auto *if_ = node->mutable_subnode()->mutable_if_();
      auto *test = if_->mutable_test();
      test->set_op(opStaticCall);
      test->mutable_subnode()->mutable_staticcall()->set_class_("Helper");
      test->mutable_subnode()->mutable_staticcall()->set_method("getTrue");

      auto *then = if_->mutable_then();
      then->set_op(opConst);
      then->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeString);
      then->mutable_subnode()->mutable_const_()->set_val("mismatch");

      auto *else_ = if_->mutable_else_();
      else_->set_op(opConst);
      else_->mutable_subnode()->mutable_const_()->set_type(
          ConstNode_ConstType_constTypeNumber);
      else_->mutable_subnode()->mutable_const_()->set_val("123");
      else_->set_tag("long");
    };

    createGenericString(sc->add_args());
    createGenericString(sc->add_args());

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_no_match_runtime",
                                     llvm::OptimizationLevel::O0, false)
                         .get()
                         .address;
      resPtrToValue(resCall);
      assert_true(false); // Should have thrown runtime exception
    } catch (const LanguageException &e) {
      // Exception: NoMatchingOverloadException
      // Message: No matching overload found for clojure.lang.Numbers/ex_add
      assert_true(true);
    } catch (const std::exception &e) {
      fprintf(stderr, "Unexpected exception: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_regression_type_narrowing_specialized(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    // This test specifically documents the fix for the
    // "InternalInconsistencyException" caused by type mismatch in specialized
    // branches. We call "add" with arguments that are statically indeterminate
    // (Any). The dispatch logic will:
    // 1. Check if they are (int, int) at runtime.
    // 2. Unbox them if true.
    // 3. Call mock_add_int(i32, i32).
    // The InvokeManager::generateIntrinsic checks if the arguments passed to
    // mock_add_int match the IntrinsicDescription. If we don't narrow the type
    // of the unboxed TypedValue, it will still be "Any" and InvokeManager will
    // throw an exception.
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    createIndeterminateArg(sc->add_args(), "42", false);
    createIndeterminateArg(sc->add_args(), "8", false);

    try {
      auto resCall = engine
                         .compileAST(callNode, "__test_regression_narrowing",
                                     llvm::OptimizationLevel::O0, false)
                         .get()
                         .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(1050, RT_unboxInt32(result)); // mock_add_int adds 1000
      release(result);
    } catch (const std::exception &e) {
      fprintf(stderr, "Regression failure: %s\n", e.what());
      assert_true(false);
    }
  });
}

static void test_regression_phi_node_segfault(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime_full(compState);
    JITEngine engine(compState);
    // This test specifically documents the fix for the LLVM segfault
    // (SimplifyCFG crash) caused by malformed PHI nodes when a specialized
    // branch contains internal branching. "complex_add" specialization "Add"
    // (intrinsic) creates "overflow" and "no_overflow" blocks.
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("complex_add");

    createIndeterminateArg(sc->add_args(), "100", false);
    createIndeterminateArg(sc->add_args(), "200", false);

    try {
      // Optimization level O1 or higher is needed to trigger SimplifyCFG in a
      // way that crashes
      auto resCall = engine
                         .compileAST(callNode, "__test_regression_segfault",
                                     llvm::OptimizationLevel::O1, false)
                         .get()
                         .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(300, RT_unboxInt32(result)); // Real Add doesn't add 1000
      release(result);
    } catch (const std::exception &e) {
      fprintf(stderr, "Segfault regression failure: %s\n", e.what());
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_dynamic_dispatch_3tail),
      cmocka_unit_test(test_dynamic_dispatch_filtering),
      cmocka_unit_test(test_dynamic_dispatch_exhaustive),
      cmocka_unit_test(test_dynamic_dispatch_no_match),
      cmocka_unit_test(test_dynamic_dispatch_no_match_runtime),
      cmocka_unit_test(test_regression_type_narrowing_specialized),
      cmocka_unit_test(test_regression_phi_node_segfault),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
