#include "../../bridge/Exceptions.h"
#include "../../bridge/InstanceCallStub.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace std;

// Helper to load metadata from protobuf (copied from JITSafety_test.cpp)
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
    fprintf(stderr, "Could not open rt-classes.cljb\n");
    return;
  }

  try {
    auto res = engine
                   .compileAST(astClasses.nodes(0), "__classes",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;
    RTValue classes = res.toPtr<RTValue (*)()>()();
    compState.storeInternalClasses(classes);
  } catch (const LanguageException &e) {
    fprintf(stderr, "LanguageException during setup_test_metadata: %s\n",
            getExceptionString(e).c_str());
    throw;
  }
}

static rt::ThreadsafeCompilerState *gCompState = nullptr;
static rt::JITEngine *gEngine = nullptr;

static int setup_test_group(void **state) {
  initialise_memory();
  gCompState = new rt::ThreadsafeCompilerState();
  gEngine = new rt::JITEngine(*gCompState);
  setup_test_metadata(*gCompState, *gEngine);
  return 0;
}

static int teardown_test_group(void **state) {
  delete gEngine;
  delete gCompState;
  RuntimeInterface_cleanup();
  return 0;
}

static void test_async_stack_trace_friendly(void **state) {
  (void)state;
  InlineCache icSlot = {0, nullptr};
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue strObj = RT_boxPtr(String_create("test"));

    try {
      RTValue args[1] = {strObj};
      // Slow path has a peculiar semantics. It does not consume when successful
      // and it does when throws.
      InstanceCallSlowPath(&icSlot, "contas", 0, args, 0, gEngine);
      fail_msg("Expected an exception from .contas");
    } catch (const LanguageException &e) {
      string trace = getExceptionString(e, StackTraceMode::Friendly);
      cout << "Friendly Exception String:\n" << trace << endl;

      // 1. Check for the error message
      assert_non_null(
          strstr(trace.c_str(), "does not have an instance method contas"));

      // 2. Friendly mode should CULL infrastructure
      assert_null(strstr(trace.c_str(), "rt::InvokeManager"));
      assert_null(strstr(trace.c_str(), "rt::JITEngine"));
      assert_null(strstr(trace.c_str(), "InstanceCallSlowPath"));
      assert_null(strstr(trace.c_str(), "std::__1"));
      assert_null(strstr(trace.c_str(), "throwInternalInconsistencyException"));

      // 3. But it should show the main test thread (the end of the chain)
      assert_non_null(strstr(trace.c_str(), "test_async_stack_trace_friendly"));
    }
  });
}

static void test_async_stack_trace_debug(void **state) {
  (void)state;
  InlineCache icSlot = {0, nullptr};
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue strObj = RT_boxPtr(String_create("test"));

    try {
      RTValue args[1] = {strObj};
      // Slow path has a peculiar semantics. It does not consume when successful
      // and it does when throws.
      InstanceCallSlowPath(&icSlot, "contas", 0, args, 0, gEngine);
      fail_msg("Expected an exception from .contas");
    } catch (const LanguageException &e) {
      string trace = getExceptionString(e, StackTraceMode::Debug);
      cout << "Debug Exception String:\n" << trace << endl;

      // 1. Debug mode should show EVERYTHING
      assert_non_null(strstr(trace.c_str(), "rt::InvokeManager"));
      assert_non_null(strstr(trace.c_str(), "InstanceCallSlowPath"));
      assert_non_null(strstr(trace.c_str(), "test_async_stack_trace_debug"));
    }
  });
}

int main() {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_async_stack_trace_friendly),
      cmocka_unit_test(test_async_stack_trace_debug),
  };
  return cmocka_run_group_tests(tests, setup_test_group, teardown_test_group);
}
