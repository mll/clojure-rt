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

static void test_double_add_repro(void **state) {
  (void)state;
  rt::ThreadsafeCompilerState compState;
  JITEngine engine(compState);

  // (def a "aa")
  {
    RTValue k = Keyword_create(String_create("a"));
    Var *v = Var_create(k);
    RTValue str = RT_boxPtr(String_create("aa"));

    Ptr_retain(v);
    Var_bindRoot(v, str);
    compState.varRegistry.registerObject("a", v);
  }

  // (+ a a)
  // We'll simulate this by a call to Numbers_add(a, a)
  // Since we don't have a direct '+' operator in bytecode that maps to Numbers_add easily without a lot of setup,
  // we can use a StaticCall to a hypothetical Numbers class or just manually trigger the codegen that leads to Numbers_add.
  // Actually, the easiest way is to use a StaticCall to something that throws or just a known runtime function.
  // The user's IR shows a complex static call dispatch.
  
  Node callNode;
  callNode.set_op(opStaticCall);
  auto *sc = callNode.mutable_subnode()->mutable_staticcall();
  sc->set_class_("clojure.lang.Numbers");
  sc->set_method("add");

  auto *arg1 = sc->add_args();
  arg1->set_op(opVar);
  arg1->mutable_subnode()->mutable_var()->set_var("v_a");

  auto *arg2 = sc->add_args();
  arg2->set_op(opVar);
  arg2->mutable_subnode()->mutable_var()->set_var("v_a");

  // We need to register clojure.lang.Numbers and its add method
  {
    String *nameStr = String_create("clojure.lang.Numbers");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);

    ClassDescription *ext = new ClassDescription();
    ext->name = "clojure.lang.Numbers";

    IntrinsicDescription id;
    id.symbol = "Numbers_add";
    id.type = CallType::Call; // Use Call to trigger invokeRuntime
    id.returnType = ObjectTypeSet::all();
    id.argTypes = {ObjectTypeSet::all(), ObjectTypeSet::all()};
    ext->staticFns["add"].push_back(id);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
  }

  try {
    auto resCall = engine
                       .compileAST(callNode, "__test_double_add",
                                   llvm::OptimizationLevel::O0, true)
                       .get();
    RTValue result = resPtrToValue(resCall);
    (void)result;
    release(result);
  } catch (const std::exception &e) {
    printf("Caught expected exception: %s\n", e.what());
  }
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_double_add_repro),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
