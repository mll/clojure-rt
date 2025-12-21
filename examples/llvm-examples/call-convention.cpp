#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;

void createProfessionalSignature(LLVMContext &Ctx, Module *M) {
    IRBuilder<> Builder(Ctx);
    Type* Int32Ty = Type::getInt32Ty(Ctx);

    // 1. Define the "Mega Context" Struct
    // This holds things that aren't the "hot" 20 variables
    std::vector<Type*> ContextFields = {
        Int32Ty,            // Varargs count
        Int32Ty->getPointerTo(), // Closure variables (pointer to array)
        Int32Ty             // Deopt status code
    };
    StructType* ContextTy = StructType::create(Ctx, ContextFields, "RuntimeContext");

    // 2. Build the parameter list: 20 integers + 1 Context Pointer
    std::vector<Type*> ParamTypes;
    for (int i = 0; i < 20; ++i) {
        ParamTypes.push_back(Int32Ty);
    }
    ParamTypes.push_back(ContextTy->getPointerTo()); // The 21st argument

    FunctionType* FT = FunctionType::get(Int32Ty, ParamTypes, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, "user_function", M);

    // 3. Performance Optimization: Attributes
    // We tell LLVM that the context pointer is never null and is read-only in this scope
    F->addParamAttr(20, Attribute::NonNull);
    F->addParamAttr(20, Attribute::NoAlias); // Promise it doesn't overlap with other pointers

    // 4. Using the variables
    BasicBlock* Entry = BasicBlock::Create(Ctx, "entry", F);
    Builder.SetInsertPoint(Entry);

    // Accessing Arg 0 (Register RDI)
    Value* FirstVar = F->getArg(0);
    // Accessing Arg 19 (Stack)
    Value* LastVar = F->getArg(19);
    // Accessing the Context (Register R10 or similar depending on CC)
    Value* ContextPtr = F->getArg(20);

    // Example: Read a closure variable from the context
    Value* ClosureBaseGep = Builder.CreateStructGEP(ContextTy, ContextPtr, 1);
    Value* ClosureBase = Builder.CreateLoad(Int32Ty->getPointerTo(), ClosureBaseGep);
    
    // Do math...
    Value* Res = Builder.CreateAdd(FirstVar, LastVar, "sum");
    Builder.CreateRet(Res);

    verifyFunction(*F);
}
