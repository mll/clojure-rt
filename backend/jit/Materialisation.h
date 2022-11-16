#ifndef LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_MATERIALISATION_H
#define LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_MATERIALISATION_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/EPCIndirectionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>
#include "../protobuf/bytecode.pb.h"
#include "ASTLayer.h"

using namespace clojure::rt::protobuf::bytecode;

class ClojureASTMaterializationUnit : public llvm::orc::MaterializationUnit {  
  ClojureASTLayer &L;
  std::unique_ptr<FunctionJIT> F;

  void discard(const llvm::orc::JITDylib &JD, const llvm::orc::SymbolStringPtr &Sym) override;
 public:

  ClojureASTMaterializationUnit(ClojureASTLayer &L, std::unique_ptr<FunctionJIT> F);      
  llvm::StringRef getName() const override;
  void materialize(std::unique_ptr<llvm::orc::MaterializationResponsibility> R) override;
};

#endif
