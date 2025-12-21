
#include "jit.h"
#include <iostream>
#include "llvm-passes/RetainRelease.h"

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
  ExitOnError ExitOnErr;
  auto RT = MainJD.getDefaultResourceTracker();
  ExitOnErr(RT->remove());
  if (auto Err = ES->endSession())
    ES->reportError(std::move(Err));
  if (auto Err = EPCIU->cleanup())
    ES->reportError(std::move(Err));
}

shared_ptr<ProgrammeState> ClojureJIT::getProgramme() {
  return ASTLayer.TheProgramme;
}

Expected<std::unique_ptr<ClojureJIT>> ClojureJIT::Create(shared_ptr<ProgrammeState> TheProgramme) {
  auto EPC = SelfExecutorProcessControl::Create();
  if (!EPC)
    return EPC.takeError();
  
  auto ES = std::make_unique<ExecutionSession>(std::move(*EPC));
  
  auto EPCIU = EPCIndirectionUtils::Create(ES->getExecutorProcessControl());
  if (!EPCIU)
    return EPCIU.takeError();

  (*EPCIU)->createLazyCallThroughManager(*ES, llvm::orc::ExecutorAddr(pointerToJITTargetAddress(&handleLazyCallThroughError)));
  
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


Expected<ExecutorSymbolDef> ClojureJIT::lookup(StringRef Name) {
  return ES->lookup({&MainJD}, Mangle(Name.str()));
}


llvm::Expected<llvm::orc::ThreadSafeModule> ClojureJIT::optimiseModule(
    llvm::orc::ThreadSafeModule TSM,
    const llvm::orc::MaterializationResponsibility &R) {
    
    // The core logic is wrapped in withModuleDo to safely access the Module.
    TSM.withModuleDo([](llvm::Module &M) {
        // For debugging: print the module before optimization.
        // M.print(llvm::errs(), nullptr);

        // 1. Create the new pass manager infrastructure.
        // These analysis managers are used by the passes to query IR properties.
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        // 2. Create the PassBuilder and register the analysis managers.
        // The PassBuilder simplifies the process of setting up the analysis pipeline.
        llvm::PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // 3. Create a FunctionPassManager to hold the function-level optimizations.
        // All passes that operate on a per-function basis will be added here.
        llvm::FunctionPassManager FPM;
        
        // --- Add Passes ---
        // The `create...Pass()` functions are replaced by direct instantiation
        // of the pass classes.

        FPM.addPass(RetainReleasePass());

        // Corresponds to: createPromoteMemoryToRegisterPass()
        FPM.addPass(llvm::SROAPass(llvm::SROAOptions()));  
        // Corresponds to: createInstructionCombiningPass()
        FPM.addPass(llvm::InstCombinePass());
        // Corresponds to: createReassociatePass()
        FPM.addPass(llvm::ReassociatePass());
        // Corresponds to: createGVNPass()
        FPM.addPass(llvm::GVNPass());
        // Corresponds to: createCFGSimplificationPass()
        FPM.addPass(llvm::SimplifyCFGPass());
        // Corresponds to: createAggressiveDCEPass()
        FPM.addPass(llvm::ADCEPass());

        // Note: `createSpeculativeExecutionIfHasBranchDivergencePass()` is a very
        // old and specific pass. Modern LLVM integrates speculation into other
        // passes like EarlyCSE and GVN. A dedicated pass is usually not required.

        // Corresponds to: createJumpThreadingPass()
        FPM.addPass(llvm::JumpThreadingPass());
        // Corresponds to: createCorrelatedValuePropagationPass()
        FPM.addPass(llvm::CorrelatedValuePropagationPass());

        // --- Vectorization Passes ---
        // Corresponds to: createLoopVectorizePass()
        FPM.addPass(llvm::LoopVectorizePass());
        // Corresponds to: createSLPVectorizerPass()
        FPM.addPass(llvm::SLPVectorizerPass());

        // Note: `createLoadStoreVectorizerPass()` and `createVectorCombinePass()`
        // have been superseded by improvements in the SLP and Loop vectorizers
        // and are no longer added explicitly.

        // Corresponds to: createTailCallEliminationPass()
        FPM.addPass(llvm::TailCallElimPass());        

        // 4. Create a ModulePassManager and add the function pass pipeline to it.
        // We use an adaptor to run the FunctionPassManager on each function in the module.
        llvm::ModulePassManager MPM;
        MPM.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(FPM)));
        
        // It's good practice to run the Verifier after optimizations.
        MPM.addPass(llvm::VerifierPass());

        // 5. Run the optimization pipeline on the module.
        MPM.run(M, MAM);

        // For debugging: print the module after optimization.
        // M.print(llvm::errs(), nullptr);
    });

    return std::move(TSM);
}


