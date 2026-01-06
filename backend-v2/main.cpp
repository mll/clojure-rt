#include <iostream>
#include <fstream>
#include "RuntimeHeaders.h"
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

#include "state/ThreadsafeState.h"
#include "jit/JITEngine.h"

using namespace std;
using namespace llvm;

#include "RuntimeHeaders.h"

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
  
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  clojure::rt::protobuf::bytecode::Programme astInterfaces;
  {
    fstream interfacesInput("rt_protocols.cljb", ios::in | ios::binary);
    if (!astInterfaces.ParseFromIstream(&interfacesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    fstream classesInput("rt_classes.cljb", ios::in | ios::binary);
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

    RTValue interfaces =
        engine.compileAST(astInterfaces.nodes(1), "__interfaces",
                          llvm::OptimizationLevel::O0,
                          false)
      .get()
      .toPtr<RTValue (*)()>()();

    RTValue classes =
        engine.compileAST(astClasses.nodes(1), "__classes",
                          llvm::OptimizationLevel::O0,
                          false)
      .get()
      .toPtr<RTValue (*)()>()();
    
    String *s = toString(interfaces);
    s = String_compactify(s);
    
    cout << "Result" << endl;
    cout << std::string(String_c_str(s)) << endl;
    cout << "!!!Result" << endl;
    Ptr_release(s);

    s = toString(classes);
    s = String_compactify(s);
    
    cout << "Result" << endl;
    cout << std::string(String_c_str(s)) << endl;
    cout << "!!!Result" << endl;
    Ptr_release(s);
    
    
    
//    release(intrinsics);
    
    // auto f = engine.compileAST(astRoot.nodes(0), "__root", llvm::OptimizationLevel::O0);    
    // cout << "Compiling!!!" << endl;
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
//    options.DsymHints.push_back(getSelfExecutablePath() + ".dSYM/Contents/Resources/DWARF/" + "clojure-rt");
    llvm::symbolize::LLVMSymbolizer symbolizer(options);
    cout << e.toString(symbolizer,
                       getSelfExecutablePath() +
                           ".dSYM/Contents/Resources/DWARF/" + "clojure-rt",
                       _dyld_get_image_vmaddr_slide(0))
         << endl;
    //e.printRawTrace();
  }    

  return 0;
}  
