#ifndef LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_ASTLAYER_H
#define LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_ASTLAYER_H

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
#include "../FunctionJIT.h"
#include "../Programme.h"


class ClojureJIT;

using namespace clojure::rt::protobuf::bytecode;
using namespace llvm::orc;
using namespace llvm;

class ClojureASTLayer {
  IRLayer &BaseLayer;
  const DataLayout &DL;
  llvm::orc::ThreadSafeModule irgenAndTakeOwnership(FunctionJIT &FnAST, const std::string &Suffix);
  shared_ptr<ProgrammeState> TheProgramme;
  ClojureJIT *TheJIT;
  unordered_map<string, bool> definedSymbols;
 public:
 
 ClojureASTLayer(IRLayer &BaseLayer, const DataLayout &DL, shared_ptr<ProgrammeState> TheProgramme, ClojureJIT *TheJIT) : BaseLayer(BaseLayer), DL(DL), TheProgramme(TheProgramme), TheJIT(TheJIT) {}
  Error add(ResourceTrackerSP RT, unique_ptr<FunctionJIT> F);  
  void emit(std::unique_ptr<MaterializationResponsibility> MR, unique_ptr<FunctionJIT> F);  
  MaterializationUnit::Interface getInterface(FunctionJIT &F);
};
    
    
#endif
