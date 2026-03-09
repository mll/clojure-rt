#include "RuntimeHeaders.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>

#include "bridge/Exceptions.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "jit/JITEngine.h"
#include "state/ThreadsafeCompilerState.h"

using namespace std;
using namespace llvm;

#include "RuntimeHeaders.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

std::string getSelfExecutablePath() {
#ifdef __APPLE__
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0)
    return std::string(path);
#elif defined(__linux__)
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count != -1) {
    return std::string(path, count);
  }
#endif
  return "";
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  if (argc != 2) {
    cout << "Please specify the filename for compilation" << endl;
    return -1;
  }

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  clojure::rt::protobuf::bytecode::Programme astInterfaces;
  {
    fstream interfacesInput("rt-protocols.cljb", ios::in | ios::binary);
    if (!astInterfaces.ParseFromIstream(&interfacesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    fstream classesInput("rt-classes.cljb", ios::in | ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  clojure::rt::protobuf::bytecode::Programme astRoot;
  {
    fstream input(argv[1], ios::in | ios::binary);
    if (!astRoot.ParseFromIstream(&input)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  initialise_memory();

  try {
    rt::ThreadsafeCompilerState state;
    rt::JITEngine engine(state);

    RTValue interfaces = engine
                             .compileAST(astInterfaces.nodes(0), "__interfaces",
                                         llvm::OptimizationLevel::O0, false)
                             .get()
                             .toPtr<RTValue (*)()>()();

    state.storeInternalProtocols(interfaces);

    RTValue classes = engine
                          .compileAST(astClasses.nodes(0), "__classes",
                                      llvm::OptimizationLevel::O0, false)
                          .get()
                          .toPtr<RTValue (*)()>()();

    state.storeInternalClasses(classes);

    //    release(intrinsics);

    // auto f = engine.compileAST(astRoot.nodes(0), "__root",
    // llvm::OptimizationLevel::O0); cout << "Compiling!!!" << endl;
    // printReferenceCounts();

    // RTValue whaat = res();
    // printReferenceCounts();
    // String *s = toString(whaat);
    // s = String_compactify(s);

    //  cout << "Result" << endl;
    //  cout << std::string(String_c_str(s)) << endl;
    //  cout << "!!!Result" << endl;
    // Ptr_release(s);
    // printReferenceCounts();
  } catch (rt::LanguageException e) {
    llvm::symbolize::LLVMSymbolizer::Options options;
    options.Demangle = true;
    options.PrintFunctions = llvm::symbolize::FunctionNameKind::LinkageName;
    //    options.DsymHints.push_back(getSelfExecutablePath() +
    //    ".dSYM/Contents/Resources/DWARF/" + "clojure-rt");
    llvm::symbolize::LLVMSymbolizer symbolizer(options);
    cout << e.toString(symbolizer,
                       getSelfExecutablePath() +
                           ".dSYM/Contents/Resources/DWARF/" + "clojure-rt",
#ifdef __APPLE__
                       _dyld_get_image_vmaddr_slide(0)
#else
                       0
#endif
                           )
         << endl;
    // e.printRawTrace();
  }

  return 0;
}
