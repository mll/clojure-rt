#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
}
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_compiler_state(rt::ThreadsafeCompilerState &compState, rt::JITEngine &engine) {
  // Load metadata
  Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  // Initialize compiler state with metadata
  llvm::orc::ExecutorAddr resClasses =
      engine
          .compileAST(astClasses.nodes(0), "__classes",
                      llvm::OptimizationLevel::O0, false)
          .get().address;
  RTValue classes = resClasses.toPtr<RTValue (*)()>()();
  auto classesList = rt::buildClasses(classes);
  for (auto &desc : classesList) {
    auto &nameStr = desc->name;
    String *className = String_create(nameStr.c_str());
    Ptr_retain(className); // Retain for the second use in Class_create
    Class *cls = Class_create(className, className, 0, nullptr);
    cls->compilerExtension = desc.release();
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(nameStr.c_str(), cls);
  }
}

static void test_integer_overflow_add(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);
    setup_compiler_state(compState, engine);

    // Create a StaticCallNode for (clojure.lang.Numbers/add 2147483647 1)
    Node callNode;
    callNode.set_op(opStaticCall);
    callNode.set_tag("long");
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("2147483647"); 

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("1");

    auto resCall = engine
                       .compileAST(callNode, "__test_overflow_add",
                                   llvm::OptimizationLevel::O0, false)
                       .get().address;

    try {
      resPtrToValue(resCall);
      fail_msg("Should have thrown ArithmeticException");
    } catch (const LanguageException &e) {
      std::string err = getExceptionString(e);
      assert_true(err.find("Integer overflow") != std::string::npos);
    }
  });
}

static void test_integer_overflow_sub(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);
    setup_compiler_state(compState, engine);

    // Create a StaticCallNode for (clojure.lang.Numbers/minus -2147483648 1)
    Node callNode;
    callNode.set_op(opStaticCall);
    callNode.set_tag("long");
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("minus");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("-2147483648"); 

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("1");

    auto resCall = engine
                       .compileAST(callNode, "__test_overflow_sub",
                                    llvm::OptimizationLevel::O0, false)
                       .get().address;

    try {
      resPtrToValue(resCall);
      fail_msg("Should have thrown ArithmeticException");
    } catch (const LanguageException &e) {
      std::string err = getExceptionString(e);
      assert_true(err.find("Integer overflow") != std::string::npos);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_integer_overflow_add),
      cmocka_unit_test(test_integer_overflow_sub),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
