#include "JITEngine.h"
#include "../RuntimeHeaders.h"
#include "../runtime/Numbers.h"
#include "../tools/EdnParser.h"
#include "bridge/ClassLookup.h"
#include "bridge/Exceptions.h"
#include "bridge/InstanceCallStub.h"
#include "bridge/Module.h"
#include "bridge/SourceLocation.h"
#include "runtime/BridgedObject.h"
#include "runtime/Ebr.h"
#include "runtime/Object.h"
#include "runtime/RTValue.h"
#include "runtime/RuntimeInterface.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include <cstdint>
#include <llvm/Analysis/InlineAdvisor.h>
#include <llvm/Analysis/InlineCost.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/ExecutionEngine/Orc/DebugObjectManagerPlugin.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Transforms/IPO/Inliner.h>
#include <llvm/Transforms/IPO/ModuleInliner.h>
#include <string>

namespace rt {

JITEngine::Finaliser::~Finaliser() {
  Ebr_leave_critical();
  Ebr_force_reclaim();
  RuntimeInterface_cleanup();
}

std::atomic<bool> JITEngine::instanceExists{false};

JITEngine::CapturedObject
JITEngine::captureObject(const llvm::MemoryBuffer &Obj) {
  std::string id = Obj.getBufferIdentifier().str();
  CapturedObject captured;
  captured.buffer = llvm::MemoryBuffer::getMemBufferCopy(Obj.getBuffer(), id);

  // Pre-parse symbols to make lookup in compileGeneric O(1) per buffer
  auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
      captured.buffer->getMemBufferRef());
  if (ObjOrErr) {
    auto &ParsedObj = *ObjOrErr.get();
    for (auto const &SymEntry : ParsedObj.symbols()) {
      auto NameOrErr = SymEntry.getName();
      if (NameOrErr) {
        llvm::StringRef symName = NameOrErr.get();
        captured.symbols.insert(symName.str());
        // Also store without the leading underscore for Mach-O targets
        if (symName.starts_with("_")) {
          captured.symbols.insert(symName.drop_front(1).str());
        }
      }
    }
  } else {
    llvm::consumeError(ObjOrErr.takeError());
  }
  return captured;
}

JITEngine::JITEngine(llvm::OptimizationLevel defaultOptLevel, bool defaultPrintIR, size_t numThreads)
    : compilationPool(std::max(numThreads / 4, size_t(1)), Priority::Low),
      executionPool(numThreads, Priority::High),
      optLevel(defaultOptLevel), printIR(defaultPrintIR) {
  if (instanceExists.exchange(true)) {
    throw std::runtime_error("Only one JITEngine instance is allowed");
  }

  // Initialise runtime.
  RuntimeInterface_initialise();
  Ebr_enter_critical();

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!JTMB)
    throw std::runtime_error("Failed to detect host");

#ifdef __linux__
  JTMB->getOptions().EmulatedTLS = true;
#else
  JTMB->getOptions().EmulatedTLS = false;
#endif

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
        this->capturedObjectBuffers.push_back(JITEngine::captureObject(*Obj));
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
  const char *paths[] = {
      "runtime/runtime_uber.bc", "../runtime/runtime_uber.bc",
      "../../runtime/runtime_uber.bc", "build/runtime/runtime_uber.bc"};

  for (const char *path : paths) {
    auto bufferOrErr = llvm::MemoryBuffer::getFile(path);
    if (bufferOrErr) {
      runtimeBitcodeBuffer = std::move(*bufferOrErr);
      break;
    }
  }

  if (!runtimeBitcodeBuffer) {
    llvm::errs() << "Warning: Could not load runtime_uber.bc from any standard "
                    "location\n";
  }
}

std::shared_future<JITResult>
JITEngine::compileGeneric(std::function<std::string(CodeGen &)> codegenFunc,
                          const std::string &moduleName,
                          bool reuseIfExists) {
  std::lock_guard<std::mutex> lock(compilationMutex);

  auto it = activeCompilations.find(moduleName);
  if (it != activeCompilations.end()) {
    return it->second;
  }

  if (reuseIfExists) {
    std::lock_guard<std::mutex> lock(engineMutex);
    if (moduleTrackers.count(moduleName)) {
      auto Sym = jit->lookup(moduleName);
      if (Sym) {
        std::promise<JITResult> p;
        p.set_value({*Sym, "", moduleName});
        return p.get_future().share();
      }
    }
  }

  // Use a shared pointer to future to ensure it stays alive even if returned
  // multiple times
  auto shared_f =
      compilationPool
          .enqueue([this, codegenFunc, moduleName]() -> JITResult {
            try {
              auto codeGenerator =
                  CodeGen(moduleName, threadsafeState, (void *)this);

              std::string fName;
              try {
                fName = codegenFunc(codeGenerator);
              } catch (...) {
                // Release constants on failure
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
              auto formMap = std::move(result.formMap);

              this->optimize(*module, fName);

              std::string ir;
              if (printIR) {
                llvm::raw_string_ostream rs(ir);
                module->print(rs, nullptr);
                llvm::errs() << ir << "\n";
              }

              llvm::orc::ThreadSafeModule TSM(std::move(module), *context);

              auto RT = jit->getMainJITDylib().createResourceTracker();

              if (auto Err = jit->addIRModule(RT, std::move(TSM))) {
                throwInternalInconsistencyException(
                    std::string("JIT Link Error: ") +
                    llvm::toString(std::move(Err)));
              }

              // Resolve IC slot addresses BEFORE the lock to avoid Materialization Deadlock
              std::vector<void *> icSlotAddresses;
              for (const auto &icName : result.icSlotNames) {
                auto icSym = jit->lookup(icName);
                if (icSym) {
                  icSlotAddresses.push_back(icSym->toPtr<void *>());
                } else {
                  llvm::consumeError(icSym.takeError());
                }
              }

              {
                std::lock_guard<std::mutex> lock(engineMutex);

                if (moduleTrackers.count(moduleName)) {
                  retireModule(moduleTrackers[moduleName]);
                }
                auto tracker =
                    new ModuleTracker(std::move(RT), std::move(constants));
                tracker->icSlotAddresses = std::move(icSlotAddresses);
                moduleTrackers[moduleName] = tracker;
              }

              auto Sym = jit->lookup(fName);

              if (!Sym) {
                throwInternalInconsistencyException("Symbol Lookup Failed: " +
                                                    fName);
              }

              {
                std::lock_guard<std::mutex> lock(this->engineMutex);

                // Find the object buffer that contains our symbol.
                // This is much more robust than name-based matching.
                size_t foundIdx = std::string::npos;
                for (size_t i = 0; i < this->capturedObjectBuffers.size();
                     ++i) {
                  if (this->capturedObjectBuffers[i].symbols.count(fName)) {
                    foundIdx = i;
                    break;
                  }
                }

                if (foundIdx != std::string::npos) {
                  auto &captured = this->capturedObjectBuffers[foundIdx];
                  // printf("JIT: Found buffer for %s (size %zu)\n",
                  // fName.c_str(), captured.buffer->getBufferSize());
                  registerJitFunction(
                      Sym->getValue(), captured.buffer->getBufferSize(),
                      fName.c_str(), captured.buffer->getBufferStart(),
                      captured.buffer->getBufferSize(), std::move(formMap));
                  this->capturedObjectBuffers.erase(
                      this->capturedObjectBuffers.begin() + foundIdx);
                } else {
                  // printf("JIT: FAILED to find buffer for %s among %zu
                  // candidates\n", fName.c_str(),
                  // this->capturedObjectBuffers.size()); Fallback for cases
                  // where symbol might not be in the dynamic symbol table of
                  // the object (though for JIT it usually is).
                  registerJitFunction(Sym->getValue(), 1024 * 1024,
                                      fName.c_str(), nullptr, 0,
                                      std::move(formMap));
                }
              }

              {
                std::lock_guard<std::mutex> lock(compilationMutex);
                activeCompilations.erase(moduleName);
              }

              return JITResult{*Sym, std::move(ir), fName};
            } catch (...) {
              std::lock_guard<std::mutex> lock(compilationMutex);
              activeCompilations.erase(moduleName);
              throw;
            }
          })
          .share();

  activeCompilations[moduleName] = shared_f;
  return shared_f;
}

std::shared_future<JITResult>
JITEngine::compileAST(const Node &AST, const std::string &moduleName) {
  return compileGeneric([&AST](CodeGen &cg) { return cg.codegenTopLevel(AST); },
                        moduleName);
}

std::shared_future<JITResult> JITEngine::compileInstanceCallBridge(
    const std::string &methodName, const ObjectTypeSet &instanceType,
    const std::vector<ObjectTypeSet> &argTypes, void *callSiteId) {
  // For bridges, the module name should be descriptive and include the call
  // site
  /* For instance calls module name is equivalent with function name */
  char buf[32];
  snprintf(buf, sizeof(buf), "_%p", callSiteId);
  std::string moduleName = "__bridge_" + methodName + "_" + std::string(buf);

  return compileGeneric(
      [moduleName, methodName, instanceType, argTypes](CodeGen &cg) {
        return cg.generateInstanceCallBridge(moduleName, methodName,
                                             instanceType, argTypes);
      },
      moduleName,
      /* we do not want to re-use the old code */ false);
}

void JITEngine::registerRuntimeSymbols() {
  using namespace llvm::orc;

  // On macOS/Darwin, symbols are prefixed with '_'
  auto &DL = jit->getDataLayout();
  auto prefix = DL.getGlobalPrefix();
  // llvm::errs() << "JIT DataLayout prefix: '" << (prefix ? prefix : ' ') <<
  // "'\n";

  auto absoluteSymbol = [&](const std::string &name, void *addr) {
    std::string mangledName = name;
    if (prefix != '\0') {
      mangledName = std::string(1, prefix) + name;
    }
    // llvm::errs() << "Registering JIT symbol: " << mangledName << " at " <<
    // addr << "\n";
    return std::make_pair(
        jit->getExecutionSession().intern(mangledName),
        ExecutorSymbolDef(ExecutorAddr::fromPtr(addr),
                          llvm::JITSymbolFlags::Exported |
                              llvm::JITSymbolFlags::Callable));
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

  // Function and Runtime Helpers
  runtimeSymbols.insert(
      absoluteSymbol("Function_extractMethod", (void *)Function_extractMethod));
  runtimeSymbols.insert(
      absoluteSymbol("RT_packVariadic", (void *)RT_packVariadic));
  runtimeSymbols.insert(
      absoluteSymbol("Ebr_flush_critical", (void *)Ebr_flush_critical));

  // The bridge:
  runtimeSymbols.insert(
      absoluteSymbol("InstanceCallSlowPath", (void *)InstanceCallSlowPath));

  runtimeSymbols.insert(
      absoluteSymbol("ClassLookupByName", (void *)ClassLookupByName));
  runtimeSymbols.insert(absoluteSymbol("ClassLookupByRegisterId",
                                       (void *)ClassLookupByRegisterId));

  runtimeSymbols.insert(
      absoluteSymbol("exceptionToString_C", (void *)exceptionToString_C));
  runtimeSymbols.insert(
      absoluteSymbol("createException_C", (void *)createException_C));
  runtimeSymbols.insert(
      absoluteSymbol("deleteException_C", (void *)deleteException_C));

  cantFail(jit->getMainJITDylib().define(
      absoluteSymbols(std::move(runtimeSymbols))));
}

void JITEngine::retireModule(const std::string &moduleName) {
  std::lock_guard<std::mutex> lock(engineMutex);
  auto it = moduleTrackers.find(moduleName);
  if (it == moduleTrackers.end()) {
    throwInternalInconsistencyException("Module not found: " + moduleName);
  }
  auto tracker = it->second;
  retireModule(tracker);
  moduleTrackers.erase(it);
}

void JITEngine::retireModule(rt::JITEngine::ModuleTracker *tracker) {
  autorelease(
      RT_boxPtr(::BridgedObject_create(tracker, &::destroyModule, this)));
}

void JITEngine::removeModule(llvm::orc::ResourceTrackerSP &tracker) {
  std::lock_guard<std::mutex> lock(engineMutex);
  cantFail(tracker->remove());
}

JITEngine::~JITEngine() {
  std::lock_guard<std::mutex> lock(engineMutex);

  for (auto const &[_, val] : moduleTrackers) {
    retireModule(val);
  }
  instanceExists.store(false);
}

void JITEngine::optimize(llvm::Module &M, const std::string &entryPoint) {
  if (llvm::verifyModule(M, &llvm::errs())) {
    throw std::runtime_error("Module verification failed before optimization");
  }

  if (optLevel != llvm::OptimizationLevel::O0 && runtimeBitcodeBuffer) {
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
          if (!G.isDeclaration() && G.hasExternalLinkage() &&
              !G.getName().starts_with("__var_ic_slot_")) {
            G.setInitializer(nullptr);
            G.setLinkage(llvm::GlobalValue::ExternalLinkage);
            if (G.isThreadLocal()) {
              if (!TM->getTargetTriple().isOSLinux()) {
                G.setThreadLocalMode(llvm::GlobalValue::LocalExecTLSModel);
              }
            }
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

        // Clear llvm.used and llvm.compiler.used to allow GlobalDCE to strip
        // fully inlined runtime functions.
        if (auto *Used = M.getGlobalVariable("llvm.used")) {
          Used->eraseFromParent();
        }
        if (auto *CompilerUsed = M.getGlobalVariable("llvm.compiler.used")) {
          CompilerUsed->eraseFromParent();
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

  if (optLevel == llvm::OptimizationLevel::O3) {
    // For O3 we want VERY aggressive inlining to pull in linked runtime
    // functions.
    llvm::InlineParams IP;
    IP = llvm::getInlineParams(3, 0);
    IP.DefaultThreshold = 2500; // VERY Aggressive
    IP.HintThreshold = 5000;
    MPM.addPass(llvm::ModuleInlinerWrapperPass(IP));
  }

  MPM.addPass(PB.buildPerModuleDefaultPipeline(optLevel));
  MPM.addPass(llvm::GlobalDCEPass());
  MPM.run(M, MAM);
}

} // namespace rt
