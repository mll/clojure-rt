#include <fstream>
#include <iostream>

#include "../RuntimeHeaders.h"
#include "../bytecode.pb.h"
#include "../jit/JITEngine.h"
#include "../state/ThreadsafeCompilerState.h"

extern "C" {
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "../runtime/Keyword.h"
#include "../runtime/PersistentVector.h"
#include "../runtime/RuntimeInterface.h"
#include "../runtime/String.h"
#include "../runtime/tests/TestTools.h"
}

#include "../tools/EdnParser.h"

static void test_trivial_memory(void **state) {
  (void)state; // unused

  ASSERT_MEMORY_ALL_BALANCED({
    RTValue val = RT_boxNil();
    release(val);
  });
}

static void test_edn_parser_memory(void **state) {
  (void)state; // unused

  // Ensure libprotobuf matches installed headers
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  rt::ThreadsafeCompilerState compState;
  rt::JITEngine engine(compState);

  llvm::orc::ExecutorAddr res =
      engine
          .compileAST(astClasses.nodes(0), "__classes",
                      llvm::OptimizationLevel::O0, false)
          .get();

  ASSERT_MEMORY_ALL_BALANCED({
    RTValue classes = res.toPtr<RTValue (*)()>()();

    // Release the final map holding the entire structure
    release(classes);
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_trivial_memory),
      cmocka_unit_test(test_edn_parser_memory),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
