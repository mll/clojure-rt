#include "JITEngine.h"
#include "../RuntimeHeaders.h"
#include "../runtime/Numbers.h"
#include "../tools/EdnParser.h"
#include "bridge/Exceptions.h"
#include "bridge/InstanceCallStub.h"
#include <llvm/Analysis/InlineAdvisor.h>
#include <llvm/Analysis/InlineCost.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/ExecutionEngine/Orc/DebugObjectManagerPlugin.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Transforms/IPO/Inliner.h>
#include <llvm/Transforms/IPO/ModuleInliner.h>

extern "C" void *__emutls_get_address(void *);

namespace rt {
 
 thread_local std::atomic<uint64_t> JITEngine::threadLocalEpoch{0};
 thread_local bool JITEngine::inSafeSection = false;

JITEngine::JITEngine(ThreadsafeCompilerState &state, size_t numThreads)
    : pool(std::max(numThreads / 4, size_t(1)), Priority::Low),
      threadsafeState(state) {

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!JTMB)
    throw std::runtime_error("Failed to detect host");

  auto TMTmp = JTMB->createTargetMachine();
  if (!TMTmp)
    throw std::runtime_error("Failed to create target machine");
  TM = std::move(*TMTmp);

  auto JITExp =
      llvm::orc::LLJITBuilder()
          .setJITTargetMachineBuilder(*JTMB)
          .setObjectLinkingLayerCreator(
              [&](llvm::orc::ExecutionSession &ES, const llvm::Triple &TT) {
                auto ObjLinkingLayer =
                    std::make_unique<llvm::orc::ObjectLinkingLayer>(ES);

                // Add debug info support
                auto Registrar = llvm::orc::createJITLoaderGDBRegistrar(ES);
                if (Registrar) {
                  ObjLinkingLayer->addPlugin(
                      std::make_unique<llvm::orc::DebugObjectManagerPlugin>(
                          ES, std::move(*Registrar)));
                }

                return ObjLinkingLayer;
              })
          .create();
  if (!JITExp)
    throw std::runtime_error("Failed to init LLJIT");
  jit = std::move(*JITExp);

  // Set up object transform to capture compiled objects
  jit->getObjTransformLayer().setTransform(
      [this](std::unique_ptr<llvm::MemoryBuffer> Obj)
          -> llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> {
        std::lock_guard<std::mutex> lock(this->engineMutex);
        std::string id = Obj->getBufferIdentifier().str();
        // Store a copy of the buffer content
        this->capturedObjectBuffers[id] =
            llvm::MemoryBuffer::getMemBufferCopy(Obj->getBuffer(), id);
        return std::move(Obj);
      });

  // Expose symbols from the current process to the JIT.
  // This is required on Linux to find runtime functions.
  auto &DL = jit->getDataLayout();
  jit->getMainJITDylib().addGenerator(
      cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          DL.getGlobalPrefix())));

  registerRuntimeSymbols();

  // Load runtime bitcode
  auto bufferOrErr = llvm::MemoryBuffer::getFile("runtime/runtime_uber.bc");
  if (!bufferOrErr) {
    llvm::errs() << "Warning: Could not load runtime/runtime_uber.bc\n";
  } else {
    runtimeBitcodeBuffer = std::move(*bufferOrErr);
  }
}

std::future<llvm::orc::ExecutorAddr>
JITEngine::compileGeneric(std::function<std::string(CodeGen &)> codegenFunc,
                          const std::string &moduleName,
                          llvm::OptimizationLevel level, bool printModule) {
  // Wrap logic in a task for the pool
  return pool.enqueue([this, codegenFunc, moduleName, level,
                       printModule]() -> llvm::orc::ExecutorAddr {
    auto codeGenerator = CodeGen(moduleName, threadsafeState, (void*)this);

    std::string fName;
    try {
      fName = codegenFunc(codeGenerator);
    } catch (...) {
      auto result = std::move(codeGenerator).release();
      for (auto c : result.constants) {
        release(c);
      }
      throw;
    }

    auto result = std::move(codeGenerator).release();
    auto context = std::move(result.context);
    auto module = std::move(result.module);
    auto constants = std::move(result.constants);

    this->optimize(*module, level, fName);

    if (printModule && module) {
      llvm::outs() << "\n=== Optimized LLVM IR for: '" << moduleName
                   << "' ===\n";
      module->print(llvm::outs(), nullptr);
      llvm::outs() << "===============================================\n";
      llvm::outs().flush();
    }

    llvm::orc::ThreadSafeModule TSM(std::move(module), *context);

    {
      std::lock_guard<std::mutex> lock(engineMutex);
      
      // If we are replacing an existing function, move the old tracker to the zombie list
      if (functionTrackers.count(moduleName)) {
        auto oldRT = functionTrackers[moduleName];
        auto oldConstants = std::move(moduleConstants[moduleName]);
        {
          std::lock_guard<std::mutex> zLock(zombieMutex);
          zombieTrackers.push_back({oldRT, globalEpoch.fetch_add(1), std::move(oldConstants)});
        }
      }

      auto RT = jit->getMainJITDylib().createResourceTracker();
 
       // ORC takes ownership of TSM and the Context inside it
       if (auto Err = jit->addIRModule(RT, std::move(TSM)))
         throwInternalInconsistencyException("JIT Link Error");
 
       functionTrackers[moduleName] = RT;
       moduleConstants[moduleName] = std::move(constants);
    }

    // Periodically sweep old modules
    sweep();

    // 6. Return executable address
    auto Sym = jit->lookup(fName);
    if (!Sym)
      throwInternalInconsistencyException("Symbol Lookup Failed: " + fName);

    // Register for stack trace resolution
    {
      std::lock_guard<std::mutex> lock(this->engineMutex);

      // Try to find the captured object buffer. ORC might append suffixes.
      auto it = this->capturedObjectBuffers.find(moduleName);
      if (it == this->capturedObjectBuffers.end()) {
        // Try common ORC suffixes if exact match fails
        it = this->capturedObjectBuffers.find(moduleName +
                                              "-jitted-objectbuffer");
      }
      if (it == this->capturedObjectBuffers.end()) {
        // Fallback: search for any key containing moduleName
        for (auto searchIt = this->capturedObjectBuffers.begin();
             searchIt != this->capturedObjectBuffers.end(); ++searchIt) {
          if (searchIt->first.find(moduleName) != std::string::npos) {
            it = searchIt;
            break;
          }
        }
      }

      if (it != this->capturedObjectBuffers.end()) {
        registerJitFunction_C(Sym->getValue(), 4096, fName.c_str(),
                              it->second->getBufferStart(),
                              it->second->getBufferSize());
        this->capturedObjectBuffers.erase(it);
      } else {
        registerJitFunction_C(Sym->getValue(), 4096, fName.c_str(), nullptr, 0);
      }
    }

    return *Sym;
  });
}

std::future<llvm::orc::ExecutorAddr>
JITEngine::compileAST(const Node &AST, const std::string &moduleName,
                      llvm::OptimizationLevel level, bool printModule) {
  return compileGeneric(
      [&AST](CodeGen &cg) { return cg.codegenTopLevel(AST); }, moduleName, level,
      printModule);
}

std::future<llvm::orc::ExecutorAddr>
JITEngine::compileInstanceCallBridge(
    const std::string &methodName, const ObjectTypeSet &instanceType,
    const std::vector<ObjectTypeSet> &argTypes, void *callSiteId,
    llvm::OptimizationLevel Level, bool printModule) {

  // For bridges, the module name should be descriptive and include the call site
  std::string moduleName = "__bridge_" + methodName + "_" +
                          std::to_string((int)instanceType.determinedType());
  if (callSiteId) {
    char buf[32];
    snprintf(buf, sizeof(buf), "_%p", callSiteId);
    moduleName += buf;
  }

  return compileGeneric(
      [methodName, instanceType, argTypes, callSiteId](CodeGen &cg) {
        return cg.generateInstanceCallBridge(methodName, instanceType, argTypes,
                                             callSiteId);
      },
      moduleName, Level, printModule);
}

void JITEngine::invalidate(const std::string &name) {
  std::lock_guard<std::mutex> lock(engineMutex);

  auto itConst = moduleConstants.find(name);
  auto itTrack = functionTrackers.find(name);

  if (itTrack != functionTrackers.end()) {
    auto RT = itTrack->second;
    std::vector<RTValue> constants;
    if (itConst != moduleConstants.end()) {
        constants = std::move(itConst->second);
        moduleConstants.erase(itConst);
    }
    
    {
        std::lock_guard<std::mutex> zLock(zombieMutex);
        zombieTrackers.push_back({RT, globalEpoch.fetch_add(1), std::move(constants)});
    }
    functionTrackers.erase(itTrack);
  }
}

void JITEngine::enterSafeSection() {
  if (inSafeSection)
    return;
  inSafeSection = true;
  threadLocalEpoch.store(globalEpoch.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);

  std::lock_guard<std::mutex> lock(zombieMutex);
  activeThreads[std::this_thread::get_id()] = &threadLocalEpoch;
}

JITEngine::SafetySection::SafetySection(JITEngine &e) : engine(e) {
  engine.enterSafeSection();
}

JITEngine::SafetySection::~SafetySection() { engine.leaveSafeSection(); }

void JITEngine::leaveSafeSection() {
  if (!inSafeSection)
    return;
  inSafeSection = false;
  threadLocalEpoch.store(0, std::memory_order_relaxed);

  std::lock_guard<std::mutex> lock(zombieMutex);
  activeThreads.erase(std::this_thread::get_id());
}

void JITEngine::sweep() {
  std::lock_guard<std::mutex> lock(zombieMutex);
  if (activeThreads.empty()) {
    // No threads in safe sections, we can clear everything
    for (auto &z : zombieTrackers) {
      cantFail(z.tracker->remove());
      for (auto v : z.constants) {
        release(v);
      }
    }
    zombieTrackers.clear();
    return;
  }

  uint64_t minEpoch = std::numeric_limits<uint64_t>::max();
  bool anyActive = false;
  for (auto const &[id, epochPtr] : activeThreads) {
    uint64_t e = epochPtr->load(std::memory_order_relaxed);
    if (e != 0) {
        if (e < minEpoch) minEpoch = e;
        anyActive = true;
    }
  }

  if (!anyActive) {
      minEpoch = globalEpoch.load(std::memory_order_relaxed);
  }

  auto it = std::remove_if(
      zombieTrackers.begin(), zombieTrackers.end(), [&](const ZombieTracker &z) {
        if (z.epoch < minEpoch) {
          cantFail(z.tracker->remove());
          for (auto v : z.constants) {
            release(v);
          }
          return true;
        }
        return false;
      });
  zombieTrackers.erase(it, zombieTrackers.end());
}

void JITEngine::registerRuntimeSymbols() {
  using namespace llvm::orc;

  // On macOS/Darwin, symbols are prefixed with '_'
  auto &DL = jit->getDataLayout();
  auto prefix = DL.getGlobalPrefix();
  // llvm::errs() << "JIT DataLayout prefix: '" << (prefix ? prefix : ' ') << "'\n";

  auto absoluteSymbol = [&](const std::string &name, void *addr) {
    std::string mangledName = name;
    if (prefix != '\0') {
      mangledName = std::string(1, prefix) + name;
    }
    // llvm::errs() << "Registering JIT symbol: " << mangledName << " at " << addr << "\n";
    return std::make_pair(jit->getExecutionSession().intern(mangledName),
                          ExecutorSymbolDef(ExecutorAddr::fromPtr(addr),
                                            llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable));
  };

  SymbolMap runtimeSymbols;

  // Numbers
  runtimeSymbols.insert(absoluteSymbol("Numbers_add", (void *)Numbers_add));
  runtimeSymbols.insert(absoluteSymbol("Numbers_sub", (void *)Numbers_sub));
  runtimeSymbols.insert(absoluteSymbol("Numbers_mul", (void *)Numbers_mul));
  runtimeSymbols.insert(absoluteSymbol("Numbers_div", (void *)Numbers_div));
  runtimeSymbols.insert(
      absoluteSymbol("Numbers_toDouble", (void *)Numbers_toDouble));
  runtimeSymbols.insert(absoluteSymbol("Numbers_lt", (void *)Numbers_lt));
  runtimeSymbols.insert(absoluteSymbol("Numbers_lte", (void *)Numbers_lte));
  runtimeSymbols.insert(absoluteSymbol("Numbers_gt", (void *)Numbers_gt));
  runtimeSymbols.insert(absoluteSymbol("Numbers_gte", (void *)Numbers_gte));
  runtimeSymbols.insert(absoluteSymbol("Numbers_equiv", (void *)Numbers_equiv));

  // Object / Memory Management
  runtimeSymbols.insert(absoluteSymbol("getType", (void *)::getType));
  runtimeSymbols.insert(absoluteSymbol("retain", (void *)retain));
  runtimeSymbols.insert(absoluteSymbol("release", (void *)release));
  runtimeSymbols.insert(absoluteSymbol("autorelease", (void *)autorelease));
  runtimeSymbols.insert(absoluteSymbol("equals", (void *)::equals));
  runtimeSymbols.insert(absoluteSymbol("hash", (void *)::hash));
  runtimeSymbols.insert(absoluteSymbol("toString", (void *)toString));
  runtimeSymbols.insert(
      absoluteSymbol("equals_managed", (void *)equals_managed));

  runtimeSymbols.insert(absoluteSymbol("Ptr_retain", (void *)Ptr_retain));
  runtimeSymbols.insert(absoluteSymbol("Ptr_release", (void *)Ptr_release));
  runtimeSymbols.insert(
      absoluteSymbol("Ptr_autorelease", (void *)Ptr_autorelease));
  runtimeSymbols.insert(absoluteSymbol("Ptr_hash", (void *)Ptr_hash));
  runtimeSymbols.insert(absoluteSymbol("Ptr_equals", (void *)Ptr_equals));

  // BigInteger
  runtimeSymbols.insert(absoluteSymbol("BigInteger_createFromInt",
                                       (void *)BigInteger_createFromInt));
  runtimeSymbols.insert(
      absoluteSymbol("BigInteger_add", (void *)BigInteger_add));
  runtimeSymbols.insert(
      absoluteSymbol("BigInteger_sub", (void *)BigInteger_sub));
  runtimeSymbols.insert(
      absoluteSymbol("BigInteger_mul", (void *)BigInteger_mul));
  runtimeSymbols.insert(
      absoluteSymbol("BigInteger_toDouble", (void *)BigInteger_toDouble));

  // Ratio
  runtimeSymbols.insert(
      absoluteSymbol("Ratio_createFromInt", (void *)Ratio_createFromInt));
  runtimeSymbols.insert(absoluteSymbol("Ratio_createFromBigInteger",
                                       (void *)Ratio_createFromBigInteger));
  runtimeSymbols.insert(absoluteSymbol("Ratio_add", (void *)Ratio_add));
  runtimeSymbols.insert(absoluteSymbol("Ratio_sub", (void *)Ratio_sub));
  runtimeSymbols.insert(absoluteSymbol("Ratio_mul", (void *)Ratio_mul));
  runtimeSymbols.insert(absoluteSymbol("Ratio_div", (void *)Ratio_div));
  runtimeSymbols.insert(
      absoluteSymbol("Ratio_toDouble", (void *)Ratio_toDouble));
  runtimeSymbols.insert(absoluteSymbol("Ratio_equals", (void *)Ratio_equals));

  // Exceptions
  runtimeSymbols.insert(absoluteSymbol("throwArithmeticException_C",
                                       (void *)throwArithmeticException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwNoMatchingOverloadException_C",
                     (void *)throwNoMatchingOverloadException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwArityException_C", (void *)throwArityException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwIllegalArgumentException_C",
                     (void *)throwIllegalArgumentException_C));
  runtimeSymbols.insert(absoluteSymbol("throwLanguageException_C",
                                       (void *)throwLanguageException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwInternalInconsistencyException_C",
                     (void *)throwInternalInconsistencyException_C));
  runtimeSymbols.insert(absoluteSymbol("throwIllegalStateException_C",
                                       (void *)throwIllegalStateException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwUnsupportedOperationException_C",
                     (void *)throwUnsupportedOperationException_C));
  runtimeSymbols.insert(
      absoluteSymbol("throwIndexOutOfBoundsException_C",
                     (void *)throwIndexOutOfBoundsException_C));

  // ClassExtension / Static Fields
  runtimeSymbols.insert(
      absoluteSymbol("ClassExtension_getIndexedStaticField",
                     (void *)ClassExtension_getIndexedStaticField));
  runtimeSymbols.insert(
      absoluteSymbol("ClassExtension_setIndexedStaticField",
                     (void *)ClassExtension_setIndexedStaticField));
  runtimeSymbols.insert(absoluteSymbol("ClassExtension_fieldIndex",
                                       (void *)ClassExtension_fieldIndex));
  runtimeSymbols.insert(
      absoluteSymbol("ClassExtension_staticFieldIndex",
                     (void *)ClassExtension_staticFieldIndex));
  runtimeSymbols.insert(
      absoluteSymbol("ClassExtension_resolveInstanceCall",
                     (void *)ClassExtension_resolveInstanceCall));

  // Dynamic Instance Call Bouncer
  runtimeSymbols.insert(
      absoluteSymbol("InstanceCallSlowPath", (void *)InstanceCallSlowPath));

  // Bridge for emulated TLS required by JIT-compiled code
  runtimeSymbols.insert(
      absoluteSymbol("___emutls_get_address", (void *)__emutls_get_address));
  runtimeSymbols.insert(
      absoluteSymbol("__emutls_get_address", (void *)__emutls_get_address));

  cantFail(jit->getMainJITDylib().define(
      absoluteSymbols(std::move(runtimeSymbols))));
}

JITEngine::~JITEngine() {
  std::vector<std::string> names;
  {
    std::lock_guard<std::mutex> lock(engineMutex);
    for (auto const &[name, _] : functionTrackers) {
      names.push_back(name);
    }
  }
  for (auto const &name : names) {
    invalidate(name);
  }
}

void JITEngine::optimize(llvm::Module &M, llvm::OptimizationLevel Level,
                         const std::string &entryPoint) {
  // No changes here, we are moving this block later.

  if (Level != llvm::OptimizationLevel::O0 && runtimeBitcodeBuffer) {
    auto runtimeModuleOrErr =
        llvm::parseBitcodeFile(*runtimeBitcodeBuffer, M.getContext());
    if (!runtimeModuleOrErr) {
      llvm::errs() << "Failed to parse runtime bitcode: "
                   << llvm::toString(runtimeModuleOrErr.takeError()) << "\n";
    } else {
      auto &runtimeModule = *runtimeModuleOrErr;

      // We want to link only what's needed and keep linked functions internal
      // so they can be inlined and then deleted if not needed elsewere.
      if (llvm::Linker::linkModules(M, std::move(runtimeModule),
                                    llvm::Linker::LinkOnlyNeeded)) {
        llvm::errs() << "Failed to link runtime bitcode\n";
      } else {
        // Shared global variables should not be duplicated (e.g., objectCount,
        // Nil_VALUE). We force them to be external declarations so they bind to
        // the host process.
        for (auto &G : M.globals()) {
          if (!G.isDeclaration() && G.hasExternalLinkage()) {
            G.setInitializer(nullptr);
            G.setLinkage(llvm::GlobalValue::ExternalLinkage);
          }
        }

        // Mark all functions from runtime as internal so they can be optimized
        // away if inlined. We keep the entry point and 'main' external so the
        // JIT can find/run them.
        for (auto &F : M) {
          if (!F.isDeclaration() && F.getName() != "main" &&
              F.getName() != entryPoint) {
            if (F.hasExternalLinkage()) {
              F.setLinkage(llvm::GlobalValue::InternalLinkage);
            }
          }
        }
      }
    }
  }

  // 1. Sync target attributes and strip blockers across the module.
  // We do this AFTER linking so that functions pulled from runtime bitcode
  // are also processed. This is critical for inlining.
  std::string bestCPU = TM->getTargetCPU().str();
  std::string bestFeatures = TM->getTargetFeatureString().str();

  // First pass: Discover "richest" attributes from the merged module.
  // Runtime bitcode usually has more detailed feature strings than the TM
  // default.
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (F.hasFnAttribute("target-cpu")) {
      auto attr = F.getFnAttribute("target-cpu").getValueAsString();
      if (attr.size() > bestCPU.size())
        bestCPU = attr.str();
    }
    if (F.hasFnAttribute("target-features")) {
      auto attr = F.getFnAttribute("target-features").getValueAsString();
      if (attr.size() > bestFeatures.size())
        bestFeatures = attr.str();
    }
  }

  // Second pass: Apply "gold standard" attributes to EVERY function.
  for (auto &F : M) {
    if (!F.isDeclaration()) {
      if (!bestCPU.empty())
        F.addFnAttr("target-cpu", bestCPU);
      if (!bestFeatures.empty())
        F.addFnAttr("target-features", bestFeatures);

      // Force consistent frame-pointer
      F.addFnAttr("frame-pointer", "non-leaf");

      F.removeFnAttr(llvm::Attribute::OptimizeNone);
      F.removeFnAttr(llvm::Attribute::NoInline);

      // Also remove ssp (stack protector) to avoid potential mismatches,
      // and ensure we don't have "no-skipped-passes" which can interfere.
      F.removeFnAttr(llvm::Attribute::StackProtect);
      F.removeFnAttr(llvm::Attribute::StackProtectReq);
      F.removeFnAttr(llvm::Attribute::StackProtectStrong);
      F.removeFnAttr("no-skipped-passes");
    }
  }

  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  llvm::PassBuilder PB(TM->getTargetTriple().isOSDarwin() ? nullptr : TM.get());

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  // Add target-specific analysis
  FAM.registerPass([&] { return TM->getTargetIRAnalysis(); });

  llvm::ModulePassManager MPM;

  if (Level == llvm::OptimizationLevel::O3) {
    // For O3 we want VERY aggressive inlining to pull in linked runtime
    // functions.
    llvm::InlineParams IP;
    IP = llvm::getInlineParams(3, 0);
    IP.DefaultThreshold = 1500; // Aggressive
    IP.HintThreshold = 2500;
    MPM.addPass(llvm::ModuleInlinerWrapperPass(IP));
  }

  MPM.addPass(PB.buildPerModuleDefaultPipeline(Level));
  MPM.run(M, MAM);
}

} // namespace rt
