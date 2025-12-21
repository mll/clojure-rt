#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <memory>

using namespace llvm;
using namespace llvm::orc;

// -----------------------------------------------------------------------------
// 1. External State Simulation
// -----------------------------------------------------------------------------

// This global variable controls our assumption.
// If == 1, assumption holds. If != 1, assumption fails.
extern "C" int global_modifier = 1;

// This is the "Downstream Function" the JIT will call.
extern "C" int get_modifier() {
    return global_modifier;
}

// -----------------------------------------------------------------------------
// 2. JIT Infrastructure Class
// -----------------------------------------------------------------------------

class SimpleJIT {
private:
    std::unique_ptr<ExecutionSession> ES;
    DataLayout DL;
    MangleAndInterner Mangle;
    RTDyldObjectLinkingLayer ObjectLayer;
    IRCompileLayer CompileLayer;
    JITDylib &MainJD;

public:
    SimpleJIT(std::unique_ptr<ExecutionSession> ES, JITTargetMachineBuilder JTMB, DataLayout DL)
        : ES(std::move(ES)), DL(std::move(DL)), Mangle(*this->ES, this->DL),
          ObjectLayer(*this->ES, []() { return std::make_unique<SectionMemoryManager>(); }),
          CompileLayer(*this->ES, ObjectLayer, std::make_unique<ConcurrentIRCompiler>(JTMB)),
          MainJD(this->ES->createBareJITDylib("<main>")) {
        
        // Expose symbols from the host process (like get_modifier) to the JIT
        MainJD.addGenerator(
            cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
                DL.getGlobalPrefix())));
    }

    static std::unique_ptr<SimpleJIT> Create() {
        auto EPC = cantFail(SelfExecutorProcessControl::Create());
        auto ES = std::make_unique<ExecutionSession>(std::move(EPC));

        JITTargetMachineBuilder JTMB(ES->getExecutorProcessControl().getTargetTriple());
        auto DL = cantFail(JTMB.getDefaultDataLayoutForTarget());

        return std::make_unique<SimpleJIT>(std::move(ES), std::move(JTMB), std::move(DL));
    }

    void addModule(std::unique_ptr<Module> M, std::unique_ptr<LLVMContext> Ctx) {
        cantFail(CompileLayer.add(MainJD, ThreadSafeModule(std::move(M), std::move(Ctx))));
    }

    ExecutorSymbol lookup(StringRef Name) {
        return cantFail(ES->lookup({&MainJD}, Mangle(Name)));
    }
};

// -----------------------------------------------------------------------------
// 3. IR Generation: The Toy Model
// -----------------------------------------------------------------------------

void createDeoptToyModel(SimpleJIT& JIT) {
    auto Ctx = std::make_unique<LLVMContext>();
    auto M = std::make_unique<Module>("DeoptDemo", *Ctx);
    M->setDataLayout(JIT.lookup("").getAddress()); // Dummy lookup to access helper, practically irrelevant here
    
    IRBuilder<> Builder(*Ctx);

    // Types
    Type* Int32Ty = Type::getInt32Ty(*Ctx);
    std::vector<Type*> TwoInts = {Int32Ty, Int32Ty};
    std::vector<Type*> ThreeInts = {Int32Ty, Int32Ty, Int32Ty};

    // ---------------------------------------------------------
    // A. Define the External Function Signature
    // ---------------------------------------------------------
    FunctionType* GetModType = FunctionType::get(Int32Ty, false);
    Function* GetModFunc = Function::Create(GetModType, Function::ExternalLinkage, "get_modifier", *M);

    // ---------------------------------------------------------
    // B. Create the Fallback (Deoptimized) Function
    // Signature: int fallback(int x, int y, int actual_modifier)
    // ---------------------------------------------------------
    FunctionType* FallbackType = FunctionType::get(Int32Ty, ThreeInts, false);
    Function* FallbackFunc = Function::Create(FallbackType, Function::ExternalLinkage, "fallback_func", *M);

    BasicBlock* BBFallback = BasicBlock::Create(*Ctx, "entry", FallbackFunc);
    Builder.SetInsertPoint(BBFallback);

    // Logic: (x + y) * modifier
    Value* FA = FallbackFunc->getArg(0); // x
    Value* FB = FallbackFunc->getArg(1); // y
    Value* FMod = FallbackFunc->getArg(2); // modifier
    Value* FSum = Builder.CreateAdd(FA, FB, "sum");
    Value* FRes = Builder.CreateMul(FSum, FMod, "res");
    Builder.CreateRet(FRes);

    // ---------------------------------------------------------
    // C. Create the Optimized Function
    // Signature: int optimized(int x, int y)
    // ---------------------------------------------------------
    FunctionType* OptType = FunctionType::get(Int32Ty, TwoInts, false);
    Function* OptFunc = Function::Create(OptType, Function::ExternalLinkage, "optimized_func", *M);

    BasicBlock* EntryBB = BasicBlock::Create(*Ctx, "entry", OptFunc);
    BasicBlock* FastBB = BasicBlock::Create(*Ctx, "fast_path", OptFunc);
    BasicBlock* DeoptBB = BasicBlock::Create(*Ctx, "deopt_path", OptFunc);

    Builder.SetInsertPoint(EntryBB);

    // 1. Get the downstream value (the assumption check)
    Value* RealMod = Builder.CreateCall(GetModFunc, {}, "current_mod");

    // 2. Verify Assumption: Is modifier == 1?
    Value* Assumption = Builder.CreateICmpEQ(RealMod, ConstantInt::get(Int32Ty, 1), "is_one");

    // 3. Branch based on assumption
    Builder.CreateCondBr(Assumption, FastBB, DeoptBB);

    // --- Fast Path ---
    // We assume modifier is 1, so we skip multiplication.
    Builder.SetInsertPoint(FastBB);
    Value* Arg1 = OptFunc->getArg(0);
    Value* Arg2 = OptFunc->getArg(1);
    
    // Pure optimization: Just add. No multiply logic exists here.
    Value* FastResult = Builder.CreateAdd(Arg1, Arg2, "fast_res");
    Builder.CreateRet(FastResult);

    // --- Deopt Path ---
    // Assumption failed! We must move to deoptimized code.
    // We construct the state required for the fallback (x, y, and the modifier we just read)
    // and transfer control.
    Builder.SetInsertPoint(DeoptBB);
    
    // In a real JIT, this might be a stackmap-based OSR. 
    // Here, we perform a "Bailout Call":
    Value* DeoptArgs[] = { Arg1, Arg2, RealMod };
    Value* DeoptResult = Builder.CreateCall(FallbackFunc, DeoptArgs, "deopt_res");
    Builder.CreateRet(DeoptResult);

    // ---------------------------------------------------------
    // Verify and Submit
    // ---------------------------------------------------------
    if (verifyModule(*M, &errs())) {
        errs() << "Error constructing function!\n";
        exit(1);
    }

    JIT.addModule(std::move(M), std::move(Ctx));
}

// -----------------------------------------------------------------------------
// 4. Main Driver
// -----------------------------------------------------------------------------

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    auto JIT = SimpleJIT::Create();
    
    // Generate the IR
    createDeoptToyModel(*JIT);

    // Look up the optimized function
    auto Sym = JIT->lookup("optimized_func");
    auto OptimizedFuncPtr = (int(*)(int, int))Sym.getAddress();

    std::cout << "--- JIT Deoptimization Demo ---\n";

    // Scenario 1: Assumption Holds
    global_modifier = 1;
    std::cout << "[Host] Setting modifier to 1 (Assumption Holds).\n";
    int res1 = OptimizedFuncPtr(10, 20); 
    // Should take 'FastBB': 10 + 20 = 30.
    std::cout << "[JIT] Result: " << res1 << " (Expected 30)\n";

    // Scenario 2: Assumption Fails
    global_modifier = 5;
    std::cout << "\n[Host] Setting modifier to 5 (Assumption Fails).\n";
    // Should take 'DeoptBB' -> Call 'fallback_func': (10 + 20) * 5 = 150.
    int res2 = OptimizedFuncPtr(10, 20);
    std::cout << "[JIT] Result: " << res2 << " (Expected 150)\n";

    return 0;
}