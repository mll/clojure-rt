#include "jit.h"
#include <iostream>

using namespace std;
using namespace llvm::orc;
using namespace llvm;

void ClojureJIT::handleLazyCallThroughError() {
  errs() << "LazyCallThrough error: Could not find function body";
  exit(1);
}

ClojureJIT::ClojureJIT(std::unique_ptr<ExecutionSession> ES, std::unique_ptr<EPCIndirectionUtils> EPCIU, JITTargetMachineBuilder JTMB, DataLayout DL, shared_ptr<ProgrammeState> TheProgramme)
  : ES(std::move(ES)), EPCIU(std::move(EPCIU)), DL(std::move(DL)),
    Mangle(*this->ES, this->DL),
    ObjectLayer(*this->ES,
                []() { return std::make_unique<SectionMemoryManager>(); }),
    CompileLayer(*this->ES, ObjectLayer,
                 std::make_unique<ConcurrentIRCompiler>(std::move(JTMB))),
    OptimiseLayer(*this->ES, CompileLayer, optimiseModule),
    ASTLayer(OptimiseLayer, this->DL, TheProgramme, this),
    MainJD(this->ES->createBareJITDylib("<main>")) {
  
  MainJD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));
}


ClojureJIT::~ClojureJIT() {
  if (auto Err = ES->endSession())
    ES->reportError(std::move(Err));
  if (auto Err = EPCIU->cleanup())
    ES->reportError(std::move(Err));
}

Expected<std::unique_ptr<ClojureJIT>> ClojureJIT::Create(shared_ptr<ProgrammeState> TheProgramme) {
  auto EPC = SelfExecutorProcessControl::Create();
  if (!EPC)
    return EPC.takeError();
  
  auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));
  
  auto EPCIU = EPCIndirectionUtils::Create(ES->getExecutorProcessControl());
  if (!EPCIU)
    return EPCIU.takeError();
  
  (*EPCIU)->createLazyCallThroughManager(*ES, pointerToJITTargetAddress(&handleLazyCallThroughError));
  
  if (auto Err = setUpInProcessLCTMReentryViaEPCIU(**EPCIU))
    return std::move(Err);
  
  JITTargetMachineBuilder JTMB(ES->getExecutorProcessControl().getTargetTriple());
  
  auto DL = JTMB.getDefaultDataLayoutForTarget();
  if (!DL)
    return DL.takeError();
  
  return std::make_unique<ClojureJIT>(std::move(ES), std::move(*EPCIU),
                                      std::move(JTMB), std::move(*DL), 
                                      TheProgramme);
}

const DataLayout &ClojureJIT::getDataLayout() const { return DL; }

JITDylib &ClojureJIT::getMainJITDylib() { return MainJD; }

Error ClojureJIT::addModule(ThreadSafeModule TSM, ResourceTrackerSP RT) {
  if (!RT)
    RT = MainJD.getDefaultResourceTracker();
  return OptimiseLayer.add(RT, std::move(TSM));
}

Error ClojureJIT::addAST(unique_ptr<FunctionJIT> F, ResourceTrackerSP RT) {
  if (!RT)
    RT = MainJD.getDefaultResourceTracker();
  return ASTLayer.add(RT, std::move(F));
}


Expected<JITEvaluatedSymbol> ClojureJIT::lookup(StringRef Name) {
  return ES->lookup({&MainJD}, Mangle(Name.str()));
}

Expected<ThreadSafeModule> ClojureJIT::optimiseModule(ThreadSafeModule TSM, const MaterializationResponsibility &R) {
  TSM.withModuleDo([](Module &M) {
//    M.print(errs(), nullptr);
    verifyModule(M);
    // Create a function pass manager.
    auto TheFPM = std::make_unique<legacy::FunctionPassManager>(&M);

    // Add some optimizations.

    // Do simple "peephole" optimizations and bit-twiddling optzns.
    TheFPM->add(createInstructionCombiningPass());
    // Reassociate expressions.
    TheFPM->add(createReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->add(createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->add(createCFGSimplificationPass());
    /* dead code */
    TheFPM->add(createAggressiveDCEPass());
    TheFPM->add(createTailCallEliminationPass());

    TheFPM->doInitialization();
    
    // Run the optimizations over all functions in the module being added to
    // the JIT.
    
    for (auto &F : M) {
      TheFPM->run(F);
    }
  });
  
  return std::move(TSM);
}
