#include "ASTLayer.h"
#include "Materialisation.h"
#include "../codegen.h"
#include "jit.h"

using namespace llvm;
using namespace llvm::orc;
using namespace std;

Error ClojureASTLayer::add(ResourceTrackerSP RT, unique_ptr<FunctionJIT> F) {
  if(definedSymbols.find(F->name) != definedSymbols.end()) return Error::success();
  definedSymbols.insert({F->name, true});
  return RT->getJITDylib().define(std::make_unique<ClojureASTMaterializationUnit>(*this, std::move(F)), RT);
}

void ClojureASTLayer::emit(std::unique_ptr<MaterializationResponsibility> MR, unique_ptr<FunctionJIT> F) {
  BaseLayer.emit(std::move(MR), irgenAndTakeOwnership(*F, ""));
}

llvm::orc::ThreadSafeModule ClojureASTLayer::irgenAndTakeOwnership(FunctionJIT &FnAST, const std::string &Suffix) {
  auto codegen = make_unique<CodeGenerator>(TheProgramme, TheJIT);
  if(FnAST.uniqueId) {
    const FnNode &node = TheProgramme->Functions.find(FnAST.uniqueId)->second.subnode().fn();
    const FnMethodNode &method = node.methods(FnAST.methodIndex).subnode().fnmethod();
    codegen->buildStaticFun(method, FnAST.name + Suffix, FnAST.retVal, FnAST.args);
  } else codegen->buildStaticFun(FnAST.method, FnAST.name + Suffix, FnAST.retVal, FnAST.args);
  return ThreadSafeModule(std::move(codegen->TheModule), std::move(codegen->TheContext));
}


MaterializationUnit::Interface ClojureASTLayer::getInterface(FunctionJIT &F) {
  MangleAndInterner Mangle(BaseLayer.getExecutionSession(), DL);
  SymbolFlagsMap Symbols;
  Symbols[Mangle(F.name)] = JITSymbolFlags(JITSymbolFlags::Exported | JITSymbolFlags::Callable);
  return MaterializationUnit::Interface(std::move(Symbols), nullptr);
}

    
