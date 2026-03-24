#include "../../bridge/Exceptions.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>

#include "../../runtime/Class.h"
#include "../../runtime/Keyword.h"
#include "../../runtime/Var.h"
#include "../../runtime/tests/TestTools.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
}
}

using namespace std;
using namespace rt;

static void setup_test_metadata(rt::ThreadsafeCompilerState &compState,
                                rt::JITEngine &engine) {
  clojure::rt::protobuf::bytecode::Programme astClasses;
  std::string path = "tests/rt-classes.cljb";
  std::fstream classesInput(path, std::ios::in | std::ios::binary);
  if (!classesInput.is_open()) {
    path = "backend-v2/tests/rt-classes.cljb";
    classesInput.open(path, std::ios::in | std::ios::binary);
  }

  if (classesInput.is_open()) {
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fprintf(stderr, "Failed to parse bytecode from %s\n", path.c_str());
      return;
    }
  } else {
    // If we can't find it, we'll try to proceed without it or fail gracefully
    fprintf(stderr, "Could not open rt-classes.cljb\n");
    return;
  }

  try {
    auto res = engine
                   .compileAST(astClasses.nodes(0), "__classes",
                               llvm::OptimizationLevel::O0, false)
                   .get();
    RTValue classes = res.address.toPtr<RTValue (*)()>()();
    compState.storeInternalClasses(classes);
  } catch (const LanguageException &e) {
    fprintf(stderr, "LanguageException during setup_test_metadata: %s\n",
            getExceptionString(e).c_str());
    throw;
  }
}

static rt::ThreadsafeCompilerState *gCompState = nullptr;
static rt::JITEngine *gEngine = nullptr;

static MemoryState gInitialMemoryState;

static int setup_test_group(void **state) {
  initialise_memory();
  captureMemoryState(&gInitialMemoryState);
  gCompState = new rt::ThreadsafeCompilerState();
  gEngine = new rt::JITEngine(*gCompState);
  setup_test_metadata(*gCompState, *gEngine);
  return 0;
}

static int teardown_test_group(void **state) {
  delete gEngine;
  delete gCompState;
  RuntimeInterface_cleanup();

  MemoryState finalMemoryState;
  captureMemoryState(&finalMemoryState);
  bool leaked = false;
  for (int i = 0; i < 256; i++) {
    if (finalMemoryState.counts[i] != gInitialMemoryState.counts[i]) {
      if (!leaked) {
        printf("\n========== Memory Leak Detected ==========\n");
        printReferenceCounts();
        leaked = true;
      }
      printf("Type %d: expected %lu, got %lu\n", i + 1,
             gInitialMemoryState.counts[i], finalMemoryState.counts[i]);
    }
  }
  if (leaked) {
    return -1;
  }

  return 0;
}

static void test_segfault_reproduction(void **state) {
  (void)state;

  // 1. Register clojure.lang.Numbers for the "bottom" call
  {
    String *nameStr = String_create("clojure.lang.Numbers");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    ClassDescription *ext = new ClassDescription();
    ext->name = "clojure.lang.Numbers";

    IntrinsicDescription addII;
    addII.symbol = "Add";
    addII.type = CallType::Intrinsic;
    addII.argTypes.push_back(ObjectTypeSet(integerType, false));
    addII.argTypes.push_back(ObjectTypeSet(integerType, false));
    addII.returnType = ObjectTypeSet(integerType, false);
    ext->staticFns["add"].push_back(addII);

    IntrinsicDescription addGeneric;
    addGeneric.symbol = "Numbers_add";
    addGeneric.type = CallType::Call;
    addGeneric.argTypes.push_back(ObjectTypeSet::all());
    addGeneric.argTypes.push_back(ObjectTypeSet::all());
    addGeneric.returnType = ObjectTypeSet::dynamicType();
    ext->staticFns["add"].push_back(addGeneric);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    gCompState->classRegistry.registerObject("clojure.lang.Numbers", cls);
  }

  // 2. Register Repro class for our JIT chain
  Class *reproCls = nullptr;
  ClassDescription *reproExt = nullptr;
  {
    String *nameStr = String_create("Repro");
    Ptr_retain(nameStr);
    reproCls = Class_create(nameStr, nameStr, 0, nullptr);
    reproExt = new ClassDescription();
    reproExt->name = "Repro";
    reproCls->compilerExtension = reproExt;
    reproCls->compilerExtensionDestructor = delete_class_description;
    gCompState->classRegistry.registerObject("Repro", reproCls);
  }

  const int NUM_FRAMES = 50;

  auto createJitFunction = [&](int depth) {
    clojure::rt::protobuf::bytecode::Node node;
    if (depth == NUM_FRAMES - 1) {
      // (+ "hello" "world") -> throws IllegalArgumentException
      node.set_op(clojure::rt::protobuf::bytecode::opStaticCall);
      auto *sc = node.mutable_subnode()->mutable_staticcall();
      sc->set_class_("clojure.lang.Numbers");
      sc->set_method("add");
      auto *arg1 = sc->add_args();
      arg1->set_op(clojure::rt::protobuf::bytecode::opConst);
      auto *c1 = arg1->mutable_subnode()->mutable_const_();
      c1->set_type(
          clojure::rt::protobuf::bytecode::ConstNode_ConstType_constTypeString);
      c1->set_val("hello");
      auto *arg2 = sc->add_args();
      arg2->set_op(clojure::rt::protobuf::bytecode::opConst);
      auto *c2 = arg2->mutable_subnode()->mutable_const_();
      c2->set_type(
          clojure::rt::protobuf::bytecode::ConstNode_ConstType_constTypeString);
      c2->set_val("world");
    } else {
      // (Repro/f{depth+1})
      node.set_op(clojure::rt::protobuf::bytecode::opStaticCall);
      auto *sc = node.mutable_subnode()->mutable_staticcall();
      sc->set_class_("Repro");
      sc->set_method("f" + to_string(depth + 1));
    }

    string modName = "repro_mod_" + to_string(depth);
    try {
      auto res =
          gEngine->compileAST(node, modName, llvm::OptimizationLevel::O0, false)
              .get();

      // Register this function as a static method in Repro class so depth-1 can
      // call it
      IntrinsicDescription desc;
      desc.symbol = res.symbolName;
      desc.type = CallType::Call;
      desc.returnType = ObjectTypeSet::dynamicType();
      reproExt->staticFns["f" + to_string(depth)].push_back(desc);

      return res.address;
    } catch (const LanguageException &e) {
      fprintf(stderr, "Compilation failed for depth %d: %s\n", depth,
              getExceptionString(e).c_str());
      throw;
    }
  };

  // Build in REVERSE order
  llvm::orc::ExecutorAddr startAddr;
  for (int i = NUM_FRAMES - 1; i >= 0; i--) {
    auto addr = createJitFunction(i);
    if (i == 0)
      startAddr = addr;
  }

  // Now call f0
  try {
    cout << "Invoking JIT chain..." << endl;
    auto f0 = startAddr.toPtr<RTValue (*)()>();
    f0();
    fail_msg("Expected exception");
  } catch (const LanguageException &e) {
    cout << "Caught exception. Starting symbolization..." << endl;
    string trace = getExceptionString(e, StackTraceMode::Debug);
    cout << "Trace length: " << trace.length() << endl;
    cout << "Trace summary:\n" << trace.substr(0, 1000) << "..." << endl;
    assert_true(trace.length() > 0);
  }
}

int main() {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_segfault_reproduction),
  };
  return cmocka_run_group_tests(tests, setup_test_group, teardown_test_group);
}
