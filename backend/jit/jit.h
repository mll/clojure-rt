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
#include "llvm/Transforms/Utils.h"

#include <memory>
#include "ASTLayer.h"
#include "../protobuf/bytecode.pb.h"
#include "../FunctionJIT.h"
#include "../Programme.h"

using namespace clojure::rt::protobuf::bytecode;

class ClojureJIT {
private:
  std::unique_ptr<llvm::orc::ExecutionSession> ES;
  std::unique_ptr<llvm::orc::EPCIndirectionUtils> EPCIU;
  
  llvm::DataLayout DL;
  llvm::orc::MangleAndInterner Mangle;
  
  llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
  llvm::orc::IRCompileLayer CompileLayer;
  
  llvm::orc::IRTransformLayer OptimiseLayer;
  ClojureASTLayer ASTLayer;
  
  llvm::orc::JITDylib &MainJD;
  
  static llvm::Expected<llvm::orc::ThreadSafeModule> optimiseModule(llvm::orc::ThreadSafeModule TSM, const llvm::orc::MaterializationResponsibility &R);
public:
  static void handleLazyCallThroughError();
  
  ClojureJIT(std::unique_ptr<llvm::orc::ExecutionSession> ES, std::unique_ptr<llvm::orc::EPCIndirectionUtils> EPCIU, llvm::orc::JITTargetMachineBuilder JTMB, llvm::DataLayout DL, std::shared_ptr<ProgrammeState> TheProgramme);
  ~ClojureJIT();
  static llvm::Expected<std::unique_ptr<ClojureJIT>> Create(std::shared_ptr<ProgrammeState> TheProgramme);
  const llvm::DataLayout &getDataLayout() const;
  llvm::orc::JITDylib &getMainJITDylib();
  
  llvm::Error addModule(llvm::orc::ThreadSafeModule TSM, llvm::orc::ResourceTrackerSP RT = nullptr);
  llvm::Error addAST(std::unique_ptr<FunctionJIT> F, llvm::orc::ResourceTrackerSP RT = nullptr);
  llvm::Expected<llvm::JITEvaluatedSymbol> lookup(llvm::StringRef Name);
  std::shared_ptr<ProgrammeState> getProgramme();
};


#endif // LLVM_EXECUTIONENGINE_ORC_CLOJUREJIT_H
