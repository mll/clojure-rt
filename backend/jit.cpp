#include "jit.h"
#include <iostream>

using namespace std;
using namespace llvm::orc;
using namespace llvm;

ClojureJIT::ClojureJIT(std::unique_ptr<ExecutionSession> ES, JITTargetMachineBuilder JTMB, DataLayout DL)
  : ES(std::move(ES)), 
    DL(std::move(DL)), 
    Mangle(*this->ES, this->DL), 
    ObjectLayer(*this->ES,[]() { 
      return std::make_unique<SectionMemoryManager>(); 
    }), 
    CompileLayer(*this->ES, ObjectLayer, std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
    MainJD(this->ES->createBareJITDylib("<main>")) {
  MainJD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));
  if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
    ObjectLayer.setOverrideObjectFlagsWithResponsibilityFlags(true);
    ObjectLayer.setAutoClaimResponsibilityForObjectSymbols(true);
  }
}

ClojureJIT::~ClojureJIT() {
  if (auto Err = ES->endSession())
    ES->reportError(std::move(Err));
}

Expected<std::unique_ptr<ClojureJIT>> ClojureJIT::Create() {
  auto EPC = SelfExecutorProcessControl::Create();
  if (!EPC)
    return EPC.takeError();
  
  auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));
  
  JITTargetMachineBuilder JTMB(ES->getExecutorProcessControl().getTargetTriple());
  
    auto DL = JTMB.getDefaultDataLayoutForTarget();
    if (!DL)
      return DL.takeError();
    
    return std::make_unique<ClojureJIT>(std::move(ES), std::move(JTMB),
                                        std::move(*DL));
}

const DataLayout &ClojureJIT::getDataLayout() const { return DL; }

JITDylib &ClojureJIT::getMainJITDylib() { return MainJD; }

Error ClojureJIT::addModule(ThreadSafeModule TSM, ResourceTrackerSP RT) {
  if (!RT)
    RT = MainJD.getDefaultResourceTracker();
  return CompileLayer.add(RT, std::move(TSM));
}

Expected<JITEvaluatedSymbol> ClojureJIT::lookup(StringRef Name) {
  return ES->lookup({&MainJD}, Mangle(Name.str()));
}
