#include "RuntimeHeaders.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

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
#include "runtime/String.h"
#include "state/ThreadsafeCompilerState.h"

using namespace std;
using namespace llvm;

#include "RuntimeHeaders.h"

extern "C" void *__emutls_get_address(void *);

int main(int argc, char *argv[]) {
  // Force the linker to include ___emutls_get_address from the Clang runtime.
  // This is required on macOS when JIT-compiled code uses thread-local storage,
  // as the JIT session needs to resolve this symbol.
  volatile void *force_emutls = (void *)&__emutls_get_address;
  (void)force_emutls;

  setbuf(stdout, NULL);
  if (argc != 2) {
    cout << "Usage: clojure-rt <filename.cljb>" << endl;
    return -1;
  }

  std::string filename = argv[1];
  llvm::OptimizationLevel optLevel = llvm::OptimizationLevel::O0;

  int retVal = -1;

  GOOGLE_PROTOBUF_VERIFY_VERSION;
  cout << "Loading protocols..." << endl;
  MemoryState initialMemoryState;
  captureMemoryState(&initialMemoryState);
  clojure::rt::protobuf::bytecode::Programme astInterfaces;
  {
    fstream interfacesInput("rt-protocols.cljb", ios::in | ios::binary);
    if (!astInterfaces.ParseFromIstream(&interfacesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }
  cout << "Loading classes..." << endl;
  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    fstream classesInput("rt-classes.cljb", ios::in | ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }
  cout << "Loading root..." << endl;
  clojure::rt::protobuf::bytecode::Programme astRoot;
  {
    fstream input(filename, ios::in | ios::binary);
    if (!astRoot.ParseFromIstream(&input)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  try {
    cout << "Initialising memory..." << endl;
    initialise_memory();
    cout << "Initialising compiler state..." << endl;
    rt::ThreadsafeCompilerState state;
    rt::JITEngine engine(state);
    cout << "Compiling interfaces..." << endl;
    RTValue interfaces = engine
                             .compileAST(astInterfaces.nodes(0), "__interfaces",
                                         llvm::OptimizationLevel::O0, false)
                             .get()
                             .address.toPtr<RTValue (*)()>()();
    cout << "Storing interfaces..." << endl;
    state.storeInternalProtocols(interfaces);

    cout << "Compiling classes..." << endl;
    RTValue classes = engine
                          .compileAST(astClasses.nodes(0), "__classes",
                                      llvm::OptimizationLevel::O0, false)
                          .get()
                          .address.toPtr<RTValue (*)()>()();
    cout << "Storing classes..." << endl;
    state.storeInternalClasses(classes);

    cout << "Compiling root..." << endl;
    for (int j = 0; j < astRoot.nodes_size(); j++) {
      auto topLevelNode = astRoot.nodes(j);
      cout << "=============================" << endl;
      cout << "Compiling!!!" << endl;
      std::string moduleName = "__repl__" + std::to_string(j);
      auto res =
          engine.compileAST(topLevelNode, moduleName, optLevel, true).get();

      if (!res.optimizedIR.empty()) {
        cout << "\n=== Optimized LLVM IR for: '" << moduleName << "' ===\n";
        cout << res.optimizedIR << "\n";
        cout << "===============================================\n" << endl;
      }

      RTValue whaat = res.address.toPtr<RTValue (*)()>()();
      String *s = toString(whaat);
      s = String_compactify(s);

      cout << "========== Result ==========" << endl;
      cout << std::string(String_c_str(s)) << endl;
      cout << "========== /Result ==========" << endl;
      Ptr_release(s);
    }
    retVal = 0;
  } catch (const rt::LanguageException &e) {
    cerr << rt::getExceptionString(e) << endl;
  } catch (std::exception e) {
    cerr << e.what() << endl;
  }

  RuntimeInterface_cleanup();

  if (strstr(BUILD_TYPE, "Debug")) {
    MemoryState finalMemoryState;
    captureMemoryState(&finalMemoryState);
    bool leaked = false;
    for (int i = 0; i < 256; i++) {
      if (finalMemoryState.counts[i] != initialMemoryState.counts[i]) {
        if (!leaked) {
          printf("\n========== Memory Leak Detected ==========\n");
          printReferenceCounts();
          leaked = true;
        }
        printf("Type %d: expected %lu, got %lu\n", i + 1,
               initialMemoryState.counts[i], finalMemoryState.counts[i]);
      }
    }
    if (leaked) {
      exit(-1);
    }
  }
  return retVal;
}
