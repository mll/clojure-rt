#include "Materialisation.h"

using namespace std;
using namespace llvm;
using namespace llvm::orc;

StringRef ClojureASTMaterializationUnit::getName() const {
  return "ClojureASTMaterializationUnit";
}

void ClojureASTMaterializationUnit::discard(const JITDylib &JD, const SymbolStringPtr &Sym) {
  llvm_unreachable("Clojure functions are not overridable");
}

ClojureASTMaterializationUnit::ClojureASTMaterializationUnit(ClojureASTLayer &L, unique_ptr<FunctionJIT> F) : MaterializationUnit(L.getInterface(*F)), L(L), F(std::move(F)) {}

void ClojureASTMaterializationUnit::materialize(std::unique_ptr<MaterializationResponsibility> R) {
  L.emit(std::move(R), std::move(F));
}
