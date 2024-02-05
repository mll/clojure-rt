#include "ASTLayer.h"
#include "Materialisation.h"
#include "../codegen.h"
#include "jit.h"

using namespace llvm;
using namespace llvm::orc;
using namespace std;

Error ClojureASTLayer::add(ResourceTrackerSP RT, unique_ptr<FunctionJIT> F) {
  if(definedSymbols.find(F->uniqueName()) != definedSymbols.end()) return Error::success();
  definedSymbols.insert({F->uniqueName(), true});
  return RT->getJITDylib().define(std::make_unique<ClojureASTMaterializationUnit>(*this, std::move(F)), RT);
}

void ClojureASTLayer::emit(std::unique_ptr<MaterializationResponsibility> MR, unique_ptr<FunctionJIT> F) {
  BaseLayer.emit(std::move(MR), irgenAndTakeOwnership(*F, ""));
}

llvm::orc::ThreadSafeModule ClojureASTLayer::irgenAndTakeOwnership(FunctionJIT &FnAST, const std::string &Suffix) {
  auto codegen = make_unique<CodeGenerator>(TheProgramme, TheJIT);
  codegen->buildStaticFun(FnAST.uniqueId, FnAST.methodIndex, codegen->getMangledUniqueFunctionName(FnAST.uniqueId) + Suffix, FnAST.retVal, FnAST.args, FnAST.closedOvers);
  return ThreadSafeModule(std::move(codegen->TheModule), std::move(codegen->TheContext));
}


MaterializationUnit::Interface ClojureASTLayer::getInterface(FunctionJIT &F) {
  MangleAndInterner Mangle(BaseLayer.getExecutionSession(), DL);
  SymbolFlagsMap Symbols;
  Symbols[Mangle(F.uniqueName())] = JITSymbolFlags(JITSymbolFlags::Exported | JITSymbolFlags::Callable);
  return MaterializationUnit::Interface(std::move(Symbols), nullptr);
}

    
