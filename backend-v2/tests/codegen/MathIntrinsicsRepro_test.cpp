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
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_mock_math(rt::ThreadsafeCompilerState &compState) {
  String *nameStr = String_create("Math");
  Ptr_retain(nameStr);
  Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

  ClassDescription *ext = new ClassDescription();
  ext->name = "Math";

  // Register "pow" as an intrinsic that maps to Math_pow
  IntrinsicDescription powIntrinsic;
  powIntrinsic.symbol = "Math_pow";
  powIntrinsic.type = CallType::Intrinsic;
  powIntrinsic.argTypes.push_back(ObjectTypeSet::dynamicType());
  powIntrinsic.argTypes.push_back(ObjectTypeSet::dynamicType());
  powIntrinsic.returnType = ObjectTypeSet(doubleType, false);
  
  ext->staticFns["pow"].push_back(powIntrinsic);

  cls->compilerExtension = ext;
  cls->compilerExtensionDestructor = delete_class_description;
  compState.classRegistry.registerObject("Math", cls);
}

static void test_math_pow_uaf(void **state) {
  (void)state;
  
  rt::ThreadsafeCompilerState compState;
  setup_mock_math(compState);
  JITEngine engine(compState);

  // (Math/pow "not-a-number" 5.0)
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("Math");
  sc->set_method("pow");
  
  auto *arg1 = sc->add_args();
  arg1->set_op(opConst);
  arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
  arg1->mutable_subnode()->mutable_const_()->set_val("not-a-number");
  
  auto *arg2 = sc->add_args();
  arg2->set_op(opConst);
  arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  arg2->mutable_subnode()->mutable_const_()->set_val("5.0");
  arg2->set_tag("double");

  try {
    std::cout << "Compiling UAF repro..." << std::endl;
    auto resCall = engine.compileAST(callNode, "repro_uaf", llvm::OptimizationLevel::O0, true).get().address;
    std::cout << "Executing UAF repro..." << std::endl;
    resPtrToValue(resCall);
    fail_msg("Should have thrown LanguageException");
  } catch (const LanguageException &e) {
    std::cout << "Caught expected LanguageException: " << e.what() << std::endl;
  } catch (const std::exception &e) {
    std::cout << "Caught expected std::exception: " << e.what() << std::endl;
  }

  std::cout << "Invalidating module, this EXPECTED TO CRASH if UAF exists..." << std::endl;
  engine.invalidate("repro_uaf");
  std::cout << "Invalidation finished (unexpectedly survived)" << std::endl;
}

static void test_math_pow_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_mock_math(compState);
    JITEngine engine(compState);

    // (Math/pow "not-a-number" "another-string")
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("Math");
    sc->set_method("pow");
    
    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    arg1->mutable_subnode()->mutable_const_()->set_val("not-a-number");
    
    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    arg2->mutable_subnode()->mutable_const_()->set_val("another-string");

    try {
      std::cout << "Compiling Leak repro..." << std::endl;
      auto resCall = engine.compileAST(callNode, "repro_leak", llvm::OptimizationLevel::O0, true).get().address;
      std::cout << "Executing Leak repro..." << std::endl;
      resPtrToValue(resCall);
    } catch (const std::exception &e) {
      std::cout << "Caught exception in Leak test: " << e.what() << std::endl;
    }
    
    engine.invalidate("repro_leak");
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_math_pow_uaf),
      cmocka_unit_test(test_math_pow_leak),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
