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

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);
  if (argc != 2) {
    cout << "Please specify the filename for compilation" << endl;
    return -1;
  }
  int retVal = -1;

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  MemoryState initialMemoryState;
  captureMemoryState(&initialMemoryState);
  initialise_memory();

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

    for (int j = 0; j < astRoot.nodes_size(); j++) {
      auto topLevelNode = astRoot.nodes(j);
      cout << "=============================" << endl;
      cout << "Compiling!!!" << endl;
      std::string moduleName = "__repl__" + std::to_string(j);
      auto f = engine.compileAST(topLevelNode, moduleName,
                                 llvm::OptimizationLevel::O0, true);

      RTValue whaat = f.get().toPtr<RTValue (*)()>()();
      String *s = toString(whaat);
      s = String_compactify(s);

      cout << "========== Result ==========" << endl;
      cout << std::string(String_c_str(s)) << endl;
      cout << "========== /Result ==========" << endl;
      Ptr_release(s);
    }
    retVal = 0;
  } catch (rt::LanguageException e) {
    cout << rt::getExceptionString(e) << endl;
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
