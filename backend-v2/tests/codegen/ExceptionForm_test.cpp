#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_compiler_state(rt::ThreadsafeCompilerState &compState,
                                 rt::JITEngine &engine) {
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
          .get()
          .address;
  RTValue classes = resClasses.toPtr<RTValue (*)()>()();
  auto classesList = rt::buildClasses(classes);
  for (auto &desc : classesList) {
    auto &nameStr = desc->name;
    String *className = String_create(nameStr.c_str());
    Ptr_retain(className);
    Class *cls = Class_create(className, className, 0, nullptr);
    cls->compilerExtension = desc.release();
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(nameStr.c_str(), cls);
  }
}

static void test_exception_form_capture(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    setup_compiler_state(engine.threadsafeState, engine);

    // Create a StaticCallNode for (/ 1 0) to trigger ArithmeticException
    Node callNode;
    callNode.set_op(opStaticCall);
    callNode.set_tag("long");
    callNode.set_form("(/ 1 0)");
    callNode.mutable_env()->set_file("test_file.clj");
    callNode.mutable_env()->set_line(42);
    callNode.mutable_env()->set_column(10);

    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method(
        "divide"); // Numbers/divide throws ArithmeticException for div by zero

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("1");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("0");

    auto resCall = engine
                       .compileAST(callNode, "__test_exception_form",
                                   llvm::OptimizationLevel::O0, false)
                       .get()
                       .address;

    try {
      resPtrToValue(resCall);
      fail_msg("Should have thrown ArithmeticException");
    } catch (const LanguageException &e) {
      std::string err = getExceptionString(e);
      cout << "Caught expected exception with form info:\n" << err << endl;
      // Verify that the form and location are present in the error string
      assert_true(err.find("(/ 1 0)") != std::string::npos);
      assert_true(err.find("test_file.clj:42:10") != std::string::npos);
      assert_true(err.find("Source context:") != std::string::npos);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exception_form_capture),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  return result;
}
