#include "jit.h"
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

using namespace std;
using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//


// Function *getFunction(std::string Name) {
//   // First, see if the function has already been added to the current module.
//   if (auto *F = TheModule->getFunction(Name))
//     return F;

//   // If not, check whether we can codegen the declaration from some existing
//   // prototype.
//   auto FI = FunctionProtos.find(Name);
//   if (FI != FunctionProtos.end())
//     return FI->second->codegen();

//   // If no existing prototype exists, return null.
//   return nullptr;
// }

// Value *NumberExprAST::codegen() {
//   return ConstantFP::get(*TheContext, APFloat(Val));
// }

// Value *VariableExprAST::codegen() {
//   // Look this variable up in the function.
//   Value *V = NamedValues[Name];
//   if (!V)
//     return LogErrorV("Unknown variable name");
//   return V;
// }

// Value *BinaryExprAST::codegen() {
//   Value *L = LHS->codegen();
//   Value *R = RHS->codegen();
//   if (!L || !R)
//     return nullptr;

//   switch (Op) {
//   case '+':
//     return Builder->CreateFAdd(L, R, "addtmp");
//   case '-':
//     return Builder->CreateFSub(L, R, "subtmp");
//   case '*':
//     return Builder->CreateFMul(L, R, "multmp");
//   case '<':
//     L = Builder->CreateFCmpULT(L, R, "cmptmp");
//     // Convert bool 0/1 to double 0.0 or 1.0
//     return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp");
//   default:
//     return LogErrorV("invalid binary operator");
//   }
// }

// Value *CallExprAST::codegen() {
//   // Look up the name in the global module table.
//   Function *CalleeF = getFunction(Callee);
//   if (!CalleeF)
//     return LogErrorV("Unknown function referenced");

//   // If argument mismatch error.
//   if (CalleeF->arg_size() != Args.size())
//     return LogErrorV("Incorrect # arguments passed");

//   std::vector<Value *> ArgsV;
//   for (unsigned i = 0, e = Args.size(); i != e; ++i) {
//     ArgsV.push_back(Args[i]->codegen());
//     if (!ArgsV.back())
//       return nullptr;
//   }

//   return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
// }

// Function *PrototypeAST::codegen() {
//   // Make the function type:  double(double,double) etc.
//   std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));
//   FunctionType *FT =
//       FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);

//   Function *F =
//       Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

//   // Set names for all arguments.
//   unsigned Idx = 0;
//   for (auto &Arg : F->args())
//     Arg.setName(Args[Idx++]);

//   return F;
// }

// Function *FunctionAST::codegen() {
//   // Transfer ownership of the prototype to the FunctionProtos map, but keep a
//   // reference to it for use below.
//   auto &P = *Proto;
//   FunctionProtos[Proto->getName()] = std::move(Proto);
//   Function *TheFunction = getFunction(P.getName());
//   if (!TheFunction)
//     return nullptr;

//   // Create a new basic block to start insertion into.
//   BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
//   Builder->SetInsertPoint(BB);

//   // Record the function arguments in the NamedValues map.
//   NamedValues.clear();
//   for (auto &Arg : TheFunction->args())
//     NamedValues[std::string(Arg.getName())] = &Arg;

//   if (Value *RetVal = Body->codegen()) {
//     // Finish off the function.
//     Builder->CreateRet(RetVal);

//     // Validate the generated code, checking for consistency.
//     verifyFunction(*TheFunction);
//     printf("Running optimiser\n");
//     // Run the optimizer on the function.
//     TheFPM->run(*TheFunction);
    
//     return TheFunction;
//   }

//   // Error reading body, remove function.
//   TheFunction->eraseFromParent();
//   return nullptr;
// }

// //===----------------------------------------------------------------------===//
// // Top-Level parsing and JIT Driver
// //===----------------------------------------------------------------------===//

// static void InitializeModuleAndPassManager() {
//   // Open a new context and module.
//   TheContext = std::make_unique<LLVMContext>();
//   TheModule = std::make_unique<Module>("my cool jit", *TheContext);
//   TheModule->setDataLayout(TheJIT->getDataLayout());

//   // Create a new builder for the module.
//   Builder = std::make_unique<IRBuilder<>>(*TheContext);

//   // Create a new pass manager attached to it.
//   TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());

//   // Do simple "peephole" optimizations and bit-twiddling optzns.
//   TheFPM->add(createInstructionCombiningPass());
//   // Reassociate expressions.
//   TheFPM->add(createReassociatePass());
//   // Eliminate Common SubExpressions.
//   TheFPM->add(createGVNPass());
//   // Simplify the control flow graph (deleting unreachable blocks, etc).
//   TheFPM->add(createCFGSimplificationPass());

//   TheFPM->doInitialization();
// }

// static void HandleDefinition() {
//   if (auto FnAST = ParseDefinition()) {
//     if (auto *FnIR = FnAST->codegen()) {
//       fprintf(stderr, "Read function definition:");
//       FnIR->print(errs());
//       fprintf(stderr, "\n");
//       ExitOnErr(TheJIT->addModule(
//           ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
//       InitializeModuleAndPassManager();
//     }
//   } else {
//     // Skip token for error recovery.
//     getNextToken();
//   }
// }

// static void HandleExtern() {
//   if (auto ProtoAST = ParseExtern()) {
//     if (auto *FnIR = ProtoAST->codegen()) {
//       fprintf(stderr, "Read extern: ");
//       FnIR->print(errs());
//       fprintf(stderr, "\n");
//       FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
//     }
//   } else {
//     // Skip token for error recovery.
//     getNextToken();
//   }
// }

// static void HandleTopLevelExpression() {
//   // Evaluate a top-level expression into an anonymous function.
//   if (auto FnAST = ParseTopLevelExpr()) {
//     if (FnAST->codegen()) {
//       // Create a ResourceTracker to track JIT'd memory allocated to our
//       // anonymous expression -- that way we can free it after executing.
//       auto RT = TheJIT->getMainJITDylib().createResourceTracker();

//       auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
//       ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
//       InitializeModuleAndPassManager();

//       // Search the JIT for the __anon_expr symbol.
//       auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

//       // Get the symbol's address and cast it to the right type (takes no
//       // arguments, returns a double) so we can call it as a native function.
//       double (*FP)() = (double (*)())(intptr_t)ExprSymbol.getAddress();
//       fprintf(stderr, "Evaluated to %f\n", FP());

//       // Delete the anonymous expression module from the JIT.
//       ExitOnErr(RT->remove());
//     }
//   } else {
//     // Skip token for error recovery.
//     getNextToken();
//   }
// }

// /// top ::= definition | external | expression | ';'
// static void MainLoop() {
//   while (true) {
//     switch (CurTok) {
//     case tok_eof:
//       return;
//     case ';': // ignore top-level semicolons.
//       getNextToken();
//       break;
//     case tok_def:
//       HandleDefinition();
//       break;
//     case tok_extern:
//       HandleExtern();
//       break;
//     default:
//       HandleTopLevelExpression();
//       break;
//     }
//   }
// }

// //===----------------------------------------------------------------------===//
// // "Library" functions that can be "extern'd" from user code.
// //===----------------------------------------------------------------------===//

// #ifdef _WIN32
// #define DLLEXPORT __declspec(dllexport)
// #else
// #define DLLEXPORT
// #endif

// /// putchard - putchar that takes a double and returns 0.
// extern "C" DLLEXPORT double putchard(double X) {
//   fputc((char)X, stderr);
//   return 0;
// }

// /// printd - printf that takes a double prints it as "%f\n", returning 0.
// extern "C" DLLEXPORT double printd(double X) {
//   fprintf(stderr, "%f\n", X);
//   return 0;
// }


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
  ExitOnError ExitOnErr;
  auto gen = CodeGenerator();
  initialise_memory();
  gettimeofday(&appp, NULL);
  printf("Initialisation time: %f\n-------------------\n", (appp.tv_sec - asss.tv_sec) + (appp.tv_usec - asss.tv_usec)/1000000.0);
  
  try {
    //cout << "Expressions: " << endl;
    struct timeval ass, app;
    gettimeofday(&ass, NULL);      
    auto expressions = gen.codegen(astRoot);
    gettimeofday(&app, NULL);
    gen.TheModule->print(errs(), nullptr);
    printf("Compile time: %f\n-------------------\n", (app.tv_sec - ass.tv_sec) + (app.tv_usec - ass.tv_usec)/1000000.0);

      //g.second->print(errs());
      //cout << endl << endl;

    printf("RESULTS:\n");
    auto RT = gen.TheJIT->getMainJITDylib().createResourceTracker();

    auto TSM = ThreadSafeModule(std::move(gen.TheModule), std::move(gen.TheContext));
    ExitOnErr(gen.TheJIT->addModule(std::move(TSM), RT));
    
    for(int i=0; i<expressions;i++) {
      struct timeval as, ap;

      
      string fname = string("__anon__") + to_string(i);
      auto ExprSymbol = ExitOnErr(gen.TheJIT->lookup(fname));
      void * (*FP)() = (void * (*)())(intptr_t)ExprSymbol.getAddress();

      gettimeofday(&as, NULL);      
      void * result = FP();
      gettimeofday(&ap, NULL);
     
      char *text = String_c_str(String_compactify(toString(result)));
      printf("Result: %s\n", text);
      printf("Time: %f\n-------------------\n", (ap.tv_sec - as.tv_sec) + (ap.tv_usec - as.tv_usec)/1000000.0);

    }
    // Get the symbol's address and cast it to the right type (takes no
    // arguments, returns a double) so we can call it as a native function.
//    fprintf(stderr, "Evaluated to %f\n", FP());
    
    // Delete the anonymous expression module from the JIT.
    printf("---------------------------");
    
    ExitOnErr(RT->remove());
    

    // Search the JIT for the __anon_expr symbol.

    


   
  } catch (CodeGenerationException e) {
    cerr << e.toString() <<endl;
    return -1;
  } catch (InternalInconsistencyException e) {
    cerr << e.toString() <<endl;
    return -1;
  }
  

  // // Prime the first token.
  // fprintf(stderr, "ready> ");

  // TheJIT = ExitOnErr(KaleidoscopeJIT::Create());

  // InitializeModuleAndPassManager();

  // // Run the main "interpreter loop" now.
  // MainLoop();

  return 0;
}
