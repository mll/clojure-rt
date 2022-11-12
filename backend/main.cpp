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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "protobuf/bytecode.pb.h"
#include <fstream>
#include "codegen.h"
#include <stdio.h>
#include <sys/time.h>
#include "jit/jit.h"

using namespace std;
using namespace llvm;
using namespace llvm::orc;

extern "C" {
  typedef struct String String; 
  String *toString(void * self);
  String *String_compactify(String *self);
  char *String_c_str(String *self);
  void initialise_memory();
}


// //===----------------------------------------------------------------------===//
// // Main driver code.
// //===----------------------------------------------------------------------===//

int main(int argc, char *argv[]) {
  if(argc != 2) {
    cout << "Please specify the filename for compilation" << endl;
    return -1;
  }
  struct timeval asss, appp;
  gettimeofday(&asss, NULL);      
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

  ExitOnError ExitOnErr;

  auto TheState = make_shared<ProgrammeState>();
  unique_ptr<ClojureJIT> TheJIT = move(*ClojureJIT::Create(TheState));

  auto RT = TheJIT->getMainJITDylib().createResourceTracker();


  gettimeofday(&appp, NULL);
  printf("Initialisation time: %f\n-------------------\n", (appp.tv_sec - asss.tv_sec) + (appp.tv_usec - asss.tv_usec)/1000000.0);
  
  try {
    //cout << "Expressions: " << endl;
    for(int j=0; j< astRoot.nodes_size(); j++) {
      auto topLevel = astRoot.nodes(j);
      auto gen = make_unique<CodeGenerator>(TheState, TheJIT.get());
 
      struct timeval ass, app;
      gettimeofday(&ass, NULL);      
      auto fname = gen->codegenTopLevel(topLevel, j);
      gettimeofday(&app, NULL);
      printf("Compile time: %f\n-------------------\n", (app.tv_sec - ass.tv_sec) + (app.tv_usec - ass.tv_usec)/1000000.0);

      auto TSM = ThreadSafeModule(std::move(gen->TheModule), std::move(gen->TheContext));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
    
      struct timeval as, ap;
      
      auto ExprSymbol = ExitOnErr(TheJIT->lookup(fname));
      void * (*FP)() = (void * (*)())(intptr_t)ExprSymbol.getAddress();
      
      gettimeofday(&as, NULL);      
      void * result = FP();
      gettimeofday(&ap, NULL);
      
      char *text = String_c_str(String_compactify(toString(result)));
      printf("Result: %s\n", text);
      printf("Time: %f\n-------------------\n", (ap.tv_sec - as.tv_sec) + (ap.tv_usec - as.tv_usec)/1000000.0);        
    }
    
    ExitOnErr(RT->remove());
    
  } catch (CodeGenerationException e) {
    cerr << e.toString() <<endl;
    return -1;
  } catch (InternalInconsistencyException e) {
    cerr << e.toString() <<endl;
    return -1;
  }
 
  return 0;
}
