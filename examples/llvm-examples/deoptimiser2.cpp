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
#include <iostream>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

// Downstream function simulation
extern "C" int global_modifier = 1;
extern "C" int get_modifier() { return global_modifier; }

// JIT Helper Class (Standard Boilerplate)
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
        MainJD.addGenerator(cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));
    }
    static std::unique_ptr<SimpleJIT> Create() {
        auto EPC = cantFail(SelfExecutorProcessControl::Create());
        auto ES = std::make_unique<ExecutionSession>(std::move(EPC));
        auto DL = cantFail(JITTargetMachineBuilder(ES->getExecutorProcessControl().getTargetTriple()).getDefaultDataLayoutForTarget());
        return std::make_unique<SimpleJIT>(std::move(ES), JITTargetMachineBuilder(ES->getExecutorProcessControl().getTargetTriple()), std::move(DL));
    }
    void addModule(std::unique_ptr<Module> M, std::unique_ptr<LLVMContext> Ctx) {
        cantFail(CompileLayer.add(MainJD, ThreadSafeModule(std::move(M), std::move(Ctx))));
    }
    ExecutorSymbol lookup(StringRef Name) { return cantFail(ES->lookup({&MainJD}, Mangle(Name))); }
};

void buildModel(SimpleJIT& JIT) {
    auto Ctx = std::make_unique<LLVMContext>();
    auto M = std::make_unique<Module>("DeoptModule", *Ctx);
    IRBuilder<> Builder(*Ctx);
    Type* Int32Ty = Type::getInt32Ty(*Ctx);

    // 1. Define the "Snapshot Struct" Type in IR
    // This is our protocol for deoptimization.
    std::vector<Type*> StructFields = { Int32Ty, Int32Ty, Int32Ty };
    StructType* StateStructTy = StructType::create(*Ctx, StructFields, "StateSnapshot");
    PointerType* StatePtrTy = StateStructTy->getPointerTo();

    // 2. Define the Fallback Function (Pure IR)
    // Signature: int fallback(StateSnapshot* state)
    FunctionType* FallbackFT = FunctionType::get(Int32Ty, { StatePtrTy }, false);
    Function* FallbackFunc = Function::Create(FallbackFT, Function::InternalLinkage, "fallback_logic", *M);
    
    BasicBlock* FBEntry = BasicBlock::Create(*Ctx, "entry", FallbackFunc);
    Builder.SetInsertPoint(FBEntry);
    
    // UNPACKING logic
    Value* StatePtr = FallbackFunc->getArg(0);
    Value* GEPA = Builder.CreateStructGEP(StateStructTy, StatePtr, 0, "gep_a");
    Value* GEPB = Builder.CreateStructGEP(StateStructTy, StatePtr, 1, "gep_b");
    Value* GEPMod = Builder.CreateStructGEP(StateStructTy, StatePtr, 2, "gep_mod");
    
    Value* ValA = Builder.CreateLoad(Int32Ty, GEPA, "load_a");
    Value* ValB = Builder.CreateLoad(Int32Ty, GEPB, "load_b");
    Value* ValMod = Builder.CreateLoad(Int32Ty, GEPMod, "load_mod");

    Value* Sum = Builder.CreateAdd(ValA, ValB);
    Value* FinalRes = Builder.CreateMul(Sum, ValMod);
    Builder.CreateRet(FinalRes);

    // 3. Define the Optimized Function (Pure IR)
    Function* GetModFunc = Function::Create(FunctionType::get(Int32Ty, false), Function::ExternalLinkage, "get_modifier", *M);
    Function* OptFunc = Function::Create(FunctionType::get(Int32Ty, {Int32Ty, Int32Ty}, false), Function::ExternalLinkage, "optimized_entry", *M);

    BasicBlock* OptEntry = BasicBlock::Create(*Ctx, "entry", OptFunc);
    BasicBlock* FastBB = BasicBlock::Create(*Ctx, "fast_path", OptFunc);
    BasicBlock* DeoptBB = BasicBlock::Create(*Ctx, "deopt_path", OptFunc);

    Builder.SetInsertPoint(OptEntry);
    // Work...
    Value* A = Builder.CreateMul(OptFunc->getArg(0), ConstantInt::get(Int32Ty, 10));
    Value* B = Builder.CreateMul(OptFunc->getArg(1), ConstantInt::get(Int32Ty, 20));
    
    // Guard
    Value* CurrentMod = Builder.CreateCall(GetModFunc);
    Value* IsOne = Builder.CreateICmpEQ(CurrentMod, ConstantInt::get(Int32Ty, 1));
    Builder.CreateCondBr(IsOne, FastBB, DeoptBB);

    // Fast Path
    Builder.SetInsertPoint(FastBB);
    Builder.CreateRet(Builder.CreateAdd(A, B));

    // Deopt Path: PACKING logic
    Builder.SetInsertPoint(DeoptBB);
    // Allocate struct on stack (alloca)
    Value* Snapshot = Builder.CreateAlloca(StateStructTy, nullptr, "snapshot");
    
    // Store variables into struct
    Builder.CreateStore(A, Builder.CreateStructGEP(StateStructTy, Snapshot, 0));
    Builder.CreateStore(B, Builder.CreateStructGEP(StateStructTy, Snapshot, 1));
    Builder.CreateStore(CurrentMod, Builder.CreateStructGEP(StateStructTy, Snapshot, 2));

    // Pass the pointer
    Value* Result = Builder.CreateCall(FallbackFunc, { Snapshot });
    Builder.CreateRet(Result);

    verifyModule(*M);
    JIT.addModule(std::move(M), std::move(Ctx));
}

int main() {
    InitializeNativeTarget(); InitializeNativeTargetAsmPrinter();
    auto JIT = SimpleJIT::Create();
    buildModel(*JIT);
    auto Fn = (int(*)(int, int))JIT->lookup("optimized_entry").getAddress();

    global_modifier = 1;
    std::cout << "Optimized: " << Fn(1, 1) << "\n"; // (10+20) = 30
    global_modifier = 3;
    std::cout << "Deoptimized: " << Fn(1, 1) << "\n"; // (10+20)*3 = 90

    return 0;
}
