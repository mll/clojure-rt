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

#include <mach-o/dyld.h>
std::string getSelfExecutablePath() {
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0)
    return std::string(path);
  return "";
}

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

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    llvm::orc::ExecutorAddr res =
        engine
            .compileAST(astClasses.nodes(0), "__classes",
                        llvm::OptimizationLevel::O0, false)
            .get();

    RTValue classes = res.toPtr<RTValue (*)()>()();

    // Release the final map holding the entire structure
    release(classes);
  });
}

static void test_edn_parser_class_parsing_memory(void **state) {
  (void)state; // unused

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    std::fstream classesInput("tests/rt-classes.cljb",
                              std::ios::in | std::ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      fail_msg("Failed to parse bytecode.");
    }
  }

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);

    llvm::orc::ExecutorAddr res =
        engine
            .compileAST(astClasses.nodes(0), "__classes",
                        llvm::OptimizationLevel::O0, false)
            .get();

    RTValue classes = res.toPtr<RTValue (*)()>()();

    {
      try {
        vector<rt::ClassDescription> classesList = rt::buildClasses(classes);
        assert_true(classesList.size() > 0);
      } catch (const rt::LanguageException &e) {
        llvm::symbolize::LLVMSymbolizer::Options options;
        options.Demangle = true;
        options.PrintFunctions = llvm::symbolize::FunctionNameKind::LinkageName;
        llvm::symbolize::LLVMSymbolizer symbolizer(options);
        cout << e.toString(symbolizer,
                           getSelfExecutablePath() +
                               ".dSYM/Contents/Resources/DWARF/" + "clojure-rt",
                           _dyld_get_image_vmaddr_slide(0))
             << endl;
        assert_true(false);
      } catch (...) {
        printf("Unknown exception caught\n");
        assert_true(false);
      }
    }
  });
}

int main(int argc, char **argv) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_trivial_memory),
      cmocka_unit_test(test_edn_parser_memory),
      cmocka_unit_test(test_edn_parser_class_parsing_memory),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
