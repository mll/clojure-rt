#ifndef LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H
#define LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H

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
#include "ASTLayer.h"
#include "../protobuf/bytecode.pb.h"
#include "../FunctionJIT.h"
#include "../Programme.h"

using namespace clojure::rt::protobuf::bytecode;
using namespace llvm;
using namespace llvm::orc;

class ClojureJIT {
private:
  std::unique_ptr<ExecutionSession> ES;
  std::unique_ptr<EPCIndirectionUtils> EPCIU;
  
  DataLayout DL;
  MangleAndInterner Mangle;
  
  RTDyldObjectLinkingLayer ObjectLayer;
  IRCompileLayer CompileLayer;
  
  IRTransformLayer OptimiseLayer;
  ClojureASTLayer ASTLayer;
  
  JITDylib &MainJD;
  
  static Expected<ThreadSafeModule> optimiseModule(ThreadSafeModule TSM, const MaterializationResponsibility &R);
public:
  static void handleLazyCallThroughError();
  
  ClojureJIT(std::unique_ptr<ExecutionSession> ES, std::unique_ptr<EPCIndirectionUtils> EPCIU, JITTargetMachineBuilder JTMB, DataLayout DL, shared_ptr<ProgrammeState> TheProgramme);
  ~ClojureJIT();
  static Expected<std::unique_ptr<ClojureJIT>> Create(shared_ptr<ProgrammeState> TheProgramme);
  const DataLayout &getDataLayout() const;
  JITDylib &getMainJITDylib();
  
  Error addModule(ThreadSafeModule TSM, ResourceTrackerSP RT = nullptr);
  Error addAST(unique_ptr<FunctionJIT> F, ResourceTrackerSP RT = nullptr);
  Expected<JITEvaluatedSymbol> lookup(StringRef Name);
};


#endif // LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H
