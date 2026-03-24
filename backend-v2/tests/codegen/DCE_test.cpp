#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include "runtime/BigInteger.h"
#include "runtime/Object.h"
#include "runtime/RTValue.h"
#include "runtime/String.h"
#include <fstream>
#include <iostream>
#include <string>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void setup_compiler_state(rt::ThreadsafeCompilerState &compState,
                                 rt::JITEngine &engine) {
  Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  auto result = engine
                    .compileAST(astClasses.nodes(0), "__classes",
                                llvm::OptimizationLevel::O0, false)
                    .get();
  RTValue classes = result.address.toPtr<RTValue (*)()>()();
  compState.storeInternalClasses(classes);
}

static Node create_bigint_def(const std::string &name, const std::string &val) {
  Node node;
  node.set_op(opDef);
  auto def = node.mutable_subnode()->mutable_def();
  def->set_name(name);
  def->set_var("user/" + name);

  Node *init = def->mutable_init();
  init->set_op(opConst);
  init->set_tag("clojure.lang.BigInt");
  auto const_ = init->mutable_subnode()->mutable_const_();
  const_->set_type(ConstNode_ConstType_constTypeNumber);
  const_->set_val(val);
  const_->set_isliteral(true);

  return node;
}

static Node create_add_vars(const std::string &v1, const std::string &v2) {
  Node node;
  node.set_op(opStaticCall);
  auto call = node.mutable_subnode()->mutable_staticcall();
  call->set_class_("clojure.lang.Numbers");
  call->set_method("add");

  Node *arg1 = call->add_args();
  arg1->set_op(opVar);
  arg1->mutable_subnode()->mutable_var()->set_var("user/" + v1);

  Node *arg2 = call->add_args();
  arg2->set_op(opVar);
  arg2->mutable_subnode()->mutable_var()->set_var("user/" + v2);

  return node;
}

static void check_no_bigint_bloat(const string &ir, const string &stepName) {
  bool foundReleaseInternal =
      ir.find("@Object_release_internal") != string::npos;
  bool foundRetainInternal = ir.find("@Object_retain_internal") != string::npos;
  bool foundBigIntAdd = ir.find("@BigInteger_add") != string::npos;

  if (foundReleaseInternal)
    cout << "Step " << stepName << " Error: found @Object_release_internal"
         << endl;
  if (foundRetainInternal)
    cout << "Step " << stepName << " Error: found @Object_retain_internal"
         << endl;
  if (foundBigIntAdd)
    cout << "Step " << stepName << " Error: found @BigInteger_add" << endl;

  assert_false(foundReleaseInternal);
  assert_false(foundRetainInternal);
  assert_false(foundBigIntAdd);
}

static void test_repl_sequence_dce(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);
    setup_compiler_state(compState, engine);

    try {
      // Step 1: (def x 3N)
      {
        cout << "Compiling Step 1: (def x 3N)" << endl;
        Node node = create_bigint_def("x", "3");
        auto result =
            engine.compileAST(node, "repl_1", llvm::OptimizationLevel::O3, true)
                .get();
        cout << "--- Step 1 IR ---\n"
             << result.optimizedIR << "\n--- End Step 1 IR ---\n";
        check_no_bigint_bloat(result.optimizedIR, "(def x 3N)");
        RTValue res = result.address.toPtr<RTValue (*)()>()();
        release(res);
      }

      // Step 2: (def y 2N)
      {
        cout << "Compiling Step 2: (def y 2N)" << endl;
        Node node = create_bigint_def("y", "2");
        auto result =
            engine.compileAST(node, "repl_2", llvm::OptimizationLevel::O3, true)
                .get();
        cout << "--- Step 2 IR ---\n"
             << result.optimizedIR << "\n--- End Step 2 IR ---\n";
        check_no_bigint_bloat(result.optimizedIR, "(def y 2N)");
        RTValue res = result.address.toPtr<RTValue (*)()>()();
        release(res);
      }

      // Step 3: (+ x y)
      {
        cout << "Compiling Step 3: (+ x y)" << endl;
        Node node = create_add_vars("x", "y");
        auto result =
            engine.compileAST(node, "repl_3", llvm::OptimizationLevel::O3, true)
                .get();
        cout << "--- Step 3 IR ---\n"
             << result.optimizedIR << "\n--- End Step 3 IR ---\n";
        check_no_bigint_bloat(result.optimizedIR, "(+ x y)");
        RTValue res = result.address.toPtr<RTValue (*)()>()();
        RTValue other = RT_boxPtr(BigInteger_createFromInt(5));
        assert_true(::equals(res, other));
        release(res);
        release(other);
      }
    } catch (const LanguageException &e) {
      cout << "Caught LanguageException: " << getExceptionString(e) << endl;
      throw;
    }
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_repl_sequence_dce),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
