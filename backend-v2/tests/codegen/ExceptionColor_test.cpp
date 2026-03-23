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
    Ptr_retain(className);
    Class *cls = Class_create(className, className, 0, nullptr);
    cls->compilerExtension = desc.release();
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(nameStr.c_str(), cls);
  }
}

static void test_exception_color_output(void **state) {
  (void)state;
  // We can't easily test isatty(STDERR_FILENO) in a test, 
  // but we can verify that the code compiles and runs.
  // We'll also check if the layout changes are present.
  
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);
    setup_compiler_state(compState, engine);

    Node callNode;
    callNode.set_op(opStaticCall);
    callNode.set_tag("long");
    callNode.set_form("(/ 1 0)");
    callNode.mutable_env()->set_file("color_test.clj");
    callNode.mutable_env()->set_line(10);
    callNode.mutable_env()->set_column(5);

    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("divide");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("1");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("0");

    auto resCall = engine
                       .compileAST(callNode, "__test_exception_color",
                                   llvm::OptimizationLevel::O0, false)
                       .get().address;

    try {
      resPtrToValue(resCall);
      fail_msg("Should have thrown ArithmeticException");
    } catch (const LanguageException &e) {
      // Force color off for string comparison if needed, but here we just print it
      std::string err = getExceptionString(e, StackTraceMode::Friendly, true);
      
      cout << "--- START OF EXCEPTION OUTPUT ---" << endl;
      cout << err << endl;
      cout << "--- END OF EXCEPTION OUTPUT ---" << endl;

      // Verify layout markers
      assert_true(err.find("[!]") != std::string::npos);
      assert_true(err.find("Exception:") != std::string::npos);
      assert_true(err.find("Message:") != std::string::npos);
      assert_true(err.find("Source context:") != std::string::npos);
      assert_true(err.find("^^^^^^^") != std::string::npos);
      assert_true(err.find("Stack Trace:") != std::string::npos);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exception_color_output),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
