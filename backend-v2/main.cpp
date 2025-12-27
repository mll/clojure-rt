#include <iostream>
#include <fstream>
#include "runtime/word.h"
#include "bytecode.pb.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
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
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "bridge/Exceptions.h"

using namespace std;
using namespace llvm;

extern "C" {
#include "runtime/ObjectProto.h"
#include "runtime/Object.h"
#include "runtime/String.h"
}

#include <mach-o/dyld.h>
std::string getSelfExecutablePath() {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        return std::string(path);
    return "";
}

int main(int argc, char *argv[]) {
  setbuf(stdout, NULL);  
  if(argc != 2) {
    cout << "Please specify the filename for compilation" << endl;
    return -1;
  }
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  clojure::rt::protobuf::bytecode::Programme astRoot;
  
  fstream input(argv[1], ios::in | ios::binary);
  if (!astRoot.ParseFromIstream(&input)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
  }

  initialise_memory();

  
  cout << "HEllo" << endl;
  try {
    throwInternalInconsistencyException("Kuku");
  } catch (rt::LanguageException e) {
    llvm::symbolize::LLVMSymbolizer::Options options;
    options.Demangle = true; 
    options.PrintFunctions = llvm::symbolize::FunctionNameKind::LinkageName;
//    options.DsymHints.push_back(getSelfExecutablePath() + ".dSYM/Contents/Resources/DWARF/" + "clojure-rt");
    llvm::symbolize::LLVMSymbolizer symbolizer(options);
    cout << e.toString(symbolizer,
                       getSelfExecutablePath() +
                           ".dSYM/Contents/Resources/DWARF/" + "clojure-rt",
                       _dyld_get_image_vmaddr_slide(0))
         << endl;
    e.printRawTrace();
  }    

  return 0;
}  
