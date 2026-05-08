#include "RuntimeHeaders.h"
#include "bytecode.pb.h"
#include <fstream>
#include <iostream>
#include <string>

#include "bridge/Exceptions.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/DebugInfo/Symbolize/Symbolize.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "jit/JITEngine.h"
#include "runtime/Ebr.h"
#include "runtime/ExecutionContext.h"
#include "runtime/String.h"
#include "runtime/Var.h"
#include "tools/RTValueWrapper.h"
#include <chrono>

using namespace std;
using namespace llvm;

#include <iomanip>

class ExecutionTimer {
  std::string name;
  std::chrono::steady_clock::time_point start;

public:
  ExecutionTimer(const std::string &n)
      : name(n), start(std::chrono::steady_clock::now()) {}
  ~ExecutionTimer() {
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "[TIMER] " << name << " took " << std::fixed
              << std::setprecision(3) << duration.count() << " ms" << std::endl;
  }
};

#include "RuntimeHeaders.h"

extern "C" void *__emutls_get_address(void *);

int main(int argc, char *argv[]) {
  // Force the linker to include ___emutls_get_address from the Clang runtime.
  // This is required on macOS when JIT-compiled code uses thread-local storage,
  // as the JIT session needs to resolve this symbol.
  volatile void *force_emutls = (void *)&__emutls_get_address;
  (void)force_emutls;

  setbuf(stdout, NULL);
  if (argc != 2) {
    cout << "Usage: clojure-rt <filename.cljb>" << endl;
    return -1;
  }

  std::string filename = argv[1];

  int retVal = -1;

  GOOGLE_PROTOBUF_VERIFY_VERSION;
  cout << "Loading protocols..." << endl;
  MemoryState initialMemoryState;
  captureMemoryState(&initialMemoryState);
  clojure::rt::protobuf::bytecode::Programme astInterfaces;
  {
    fstream interfacesInput("rt-protocols.cljb", ios::in | ios::binary);
    if (!astInterfaces.ParseFromIstream(&interfacesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }
  cout << "Loading classes..." << endl;
  clojure::rt::protobuf::bytecode::Programme astClasses;
  {
    fstream classesInput("rt-classes.cljb", ios::in | ios::binary);
    if (!astClasses.ParseFromIstream(&classesInput)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  clojure::rt::protobuf::bytecode::Programme astClassExtensions;
  {
    fstream extensionsInput("rt-classes-extension.cljb", ios::in | ios::binary);
    if (!extensionsInput.good()) {
      cerr << "Failed to open rt-classes-extension.cljb" << endl;
      return -1;
    }
    if (!astClassExtensions.ParseFromIstream(&extensionsInput)) {
      cerr << "Failed to parse extensions bytecode." << endl;
      return -1;
    }
  }

  cout << "Loading root..." << endl;
  clojure::rt::protobuf::bytecode::Programme astRoot;
  {
    fstream input(filename, ios::in | ios::binary);
    if (!astRoot.ParseFromIstream(&input)) {
      cerr << "Failed to parse bytecode." << endl;
      return -1;
    }
  }

  try {
    cout << "Initialising compiler state..." << endl;
    initialise_memory();
#ifndef NDEBUG
    rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
#else
    rt::JITEngine engine(llvm::OptimizationLevel::O3, false);
#endif

    rt::ScopedRef<ExecutionContext> ctx(
        ExecutionContext_create(RT_boxPtr(PersistentArrayMap_empty())));

    // Setup default Clojure namespaces and *ns* Var
    {
      // 1. Create "clojure.core" namespace
      Symbol *coreSym = Symbol_create(String_create("clojure.core"));
      Namespace *coreNs = Namespace_findOrCreate(coreSym); // coreNs has +1 refcount
      
      // 2. Intern "*ns*" Var in "clojure.core"
      Symbol *nsSym = Symbol_create(String_create("*ns*"));
      Ptr_retain(coreNs); // Retain coreNs because Namespace_intern consumes self
      Var *nsVar = Namespace_intern(coreNs, nsSym); // nsVar has +1 refcount. nsSym is consumed.
      Var_setDynamic(nsVar, true);
      
      // 3. Create "user" namespace
      Symbol *userSym = Symbol_create(String_create("user"));
      Namespace *userNs = Namespace_findOrCreate(userSym); // userNs has +1 refcount
      RTValue userNsVal = RT_boxPtr(userNs); // userNsVal has +1 refcount
      retain(userNsVal); // userNsVal has +2 refcount
      
      // 4. Bind clojure.core/*ns* Var root to the user namespace object
      Ptr_retain(nsVar); // Retain nsVar because Var_bindRoot consumes self
      Var_bindRoot(nsVar, userNsVal); // Consumes userNsVal (+2 -> +1) and nsVar
      
      // 5. Build clojure.core/*ns* keyword for ExecutionContext dynamic bindings map
      RTValue nsVarKw = Keyword_create(String_create("clojure.core/*ns*"));
      retain(nsVarKw); // nsVarKw has +2 refcount
      
      // Assoc clojure.core/*ns* keyword with userNsVal in ExecutionContext bindings map
      ctx->bindingsMap = RT_boxPtr(PersistentArrayMap_assoc(
          (PersistentArrayMap *)RT_unboxPtr(ctx->bindingsMap), nsVarKw,
          userNsVal)); // assoc consumes nsVarKw (+2 -> +1) and userNsVal (+1 -> +0)
      
      // 6. Register in compiler state so JIT finds this exact Var object
      engine.threadsafeState.varRegistry.registerObject("clojure.core/*ns*",
                                                        nsVar); // Registry takes ownership of nsVar (+1)
      
      // Release remaining local reference to clojure.core namespace object
      Ptr_release(coreNs);
      
      // Release remaining keyword reference
      release(nsVarKw);
    }

    {
      ExecutionTimer t("Compiling and storing interfaces");
      cout << "Compiling interfaces..." << endl;
      RTValue interfaces =
          engine.compileAST(astInterfaces.nodes(0), "__interfaces")
              .get()
              .address.toPtr<RTValue (*)()>()();
      cout << "Storing interfaces..." << endl;
      engine.threadsafeState.storeInternalProtocols(interfaces);
    }

    {
      ExecutionTimer t("Compiling and storing classes");
      cout << "Compiling classes..." << endl;
      RTValue classes = engine.compileAST(astClasses.nodes(0), "__classes")
                            .get()
                            .address.toPtr<RTValue (*)()>()();
      cout << "Storing classes..." << endl;
      engine.threadsafeState.storeInternalClasses(classes);
    }

    {
      ExecutionTimer t("Compiling and extending classes");
      cout << "Compiling class extensions..." << endl;
      RTValue extensions =
          engine.compileAST(astClassExtensions.nodes(0), "__class_extensions")
              .get()
              .address.toPtr<RTValue (*)()>()();
      cout << "Extending classes..." << endl;
      engine.threadsafeState.extendInternalClasses(extensions);
    }

    cout << "Compiling root..." << endl;
    for (int j = 0; j < astRoot.nodes_size(); j++) {
      std::string moduleName = "__repl__" + std::to_string(j);
      auto topLevelNode = astRoot.nodes(j);
      cout << "=============================" << endl;

      rt::JITResult res;
      {
        ExecutionTimer t("Compiling " + moduleName);
        cout << "Compiling!!!" << endl;
        res = engine.compileASTWithContext(topLevelNode, moduleName).get();
      }

      if (!res.optimizedIR.empty()) {
        cout << "\n=== Optimized LLVM IR for: '" << moduleName << "' ===\n";
        cout << res.optimizedIR << "\n";
        cout << "===============================================\n" << endl;
      }

      {
        ExecutionTimer t("Executing " + moduleName);
        typedef RTValue (*JitFn)(ExecutionContext *);
        JitFn fn = (JitFn)res.address.toPtr<void *>();
        RTValue whaat = fn(ctx);
        String *s = toString(whaat);
        s = String_compactify(s);

        cout << "========== Result ==========" << endl;
        cout << std::string(String_c_str(s)) << endl;
        cout << "========== /Result ==========" << endl;
        Ptr_release(s);
      }
    }
    retVal = 0;
  } catch (const rt::LanguageException &e) {
    cerr << rt::getExceptionString(e) << endl;
  } catch (const std::exception &e) {
    cerr << "Error: " << endl;
    cerr << e.what() << endl;
  }

  if (strstr(BUILD_TYPE, "Debug")) {
    MemoryState finalMemoryState;
    captureMemoryState(&finalMemoryState);
    bool leaked = false;
    for (int i = 0; i < 256; i++) {
      if (finalMemoryState.counts[i] != initialMemoryState.counts[i]) {
        if (!leaked) {
          printf("\n========== Memory Leak Detected ==========\n");
          printReferenceCounts();
          leaked = true;
        }
        printf("Type %d: expected %lu, got %lu\n", i + 1,
               initialMemoryState.counts[i], finalMemoryState.counts[i]);
      }
    }
    if (leaked) {
      exit(-1);
    }
  }
  return retVal;
}
