#ifndef CODEGEN_H
#define CODEGEN_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>        
#include <llvm/IR/Verifier.h>      
#include <llvm/IR/Type.h>          
#include <llvm/IR/DerivedTypes.h>

namespace rt {
class CodeGen {
    llvm::IRBuilder<> Builder;
    llvm::orc::ThreadSafeContext TSContext;
    llvm::LLVMContext &TheContext;
    std::unique_ptr<llvm::Module> TheModule;    
    
  using NodeHandler = void (CodeGen::*)(Node*);
  static const Handler dispatch_table[30];

public:
  CodeGen(std::string_view ModuleName)
      : TSContext(std::make_unique<llvm::LLVMContext>()),
        TheModule(std::make_unique<llvm::Module>(ModuleName,
                                                 *TSContext.getContext())),
        Builder(*TSContext.getContext()),
    TheContext(*TSContext.getContext()) {}
        
  void run(Node* n) {
    // High performance: One bounds check + one indirect jump
    (this->*dispatch_table[static_cast<int>(n->type)])(n);
  }
  
  // Declarations only (keep this file small!)
  void handleBinary(Node* n);
  void handleIf(Node* n);
  // ... 28 more
};

}




#endif
