#include <iostream>
#include <fstream>
#include "word.h"
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


using namespace std;
using namespace llvm;

extern "C" {
  typedef struct String String; 
  String *toString(void * self);
  String *String_compactify(String *self);
  char *String_c_str(String *self);
  void initialise_memory();
  void printReferenceCounts();
  void release(void *self);
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
  
  cout << "HEllo"  << endl;
  return 0;
}  
