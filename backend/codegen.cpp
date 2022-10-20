#include "codegen.h"  


CodeGenerator::CodeGenerator() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("Clojure JIT", *TheContext);
  TheJIT = std::move(ClojureJIT::Create().get());

  TheModule->setDataLayout(TheJIT->getDataLayout());
  
  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
  
  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<legacy::FunctionPassManager>(TheModule.get());
  
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());
  
  TheFPM->doInitialization();
}

Value *CodeGenerator::codegen(const Programme &programme) {
  for (int i=0; i< programme.nodes_size(); i++) {
    auto node = programme.nodes(i);
    codegen(node);
  }
  return nullptr;
}

Value *CodeGenerator::codegen(const Node &node) {
  switch (node.op()) {
  case opConst:
    return codegen(node, node.subnode().const_());
    break;
  default:
    throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  }
  return nullptr;
}

 
CodeGenerationException::CodeGenerationException(const string errorMessage, const Node& node) {
  stringstream retval;
  auto env = node.env();
  cout << env.file() <<  ":" << env.line() << ":" << env.column() << ": error: " << errorMessage << "\n";
  cout << node.form() << "\n";
  cout << string(node.form().length(), '^') << "\n";
  this->errorMessage = retval.str();
}


string CodeGenerationException::toString() {
  return errorMessage;
}

