void createFastestExceptionModel(LLVMContext &Ctx, Module *M) {
    // 1. Setup helper
    MDBuilder MDB(Ctx);

    // 2. Define Weights
    // The arguments correspond to the successors of the instruction.
    // For 'invoke': Arg1 = Normal Path, Arg2 = Unwind Path
    // We say: "The Normal path happens 1,000,000 times. The Unwind path happens 1 time."
    MDNode* BranchWeights = MDB.createBranchWeights(1000000, 1);

    IRBuilder<> Builder(Ctx);
    Type* Int32Ty = Type::getInt32Ty(Ctx);
    Type* PtrTy = Type::getInt8PtrTy(Ctx);

    // 1. Setup Types
    StructType* SnapshotTy = StructType::create(Ctx, { Int32Ty, Int32Ty, PtrTy }, "ExSnapshot"); // ID, Var, ExPtr
    StructType* LPadRetTy = StructType::get(Ctx, { PtrTy, Int32Ty }); // { i8* ExObj, i32 Selector }

    // 2. Define Functions
    Function* OptFunc = Function::Create(FunctionType::get(Int32Ty, { Int32Ty }, false), Function::ExternalLinkage, "A.optimized", *M);
    Function* ThrowingFunc = Function::Create(FunctionType::get(Int32Ty, {}, false), Function::ExternalLinkage, "some_cpp_func", *M);
    Function* FallbackFunc = Function::Create(FunctionType::get(Int32Ty, { SnapshotTy->getPointerTo() }, false), Function::ExternalLinkage, "A.fallback", *M);
    
    // Personality function is REQUIRED for 'invoke'
    Function* Personality = Function::Create(FunctionType::get(Int32Ty, {}, true), Function::ExternalLinkage, "__gxx_personality_v0", *M);
    OptFunc->setPersonalityFn(Personality);

    // 3. Blocks
    BasicBlock* Entry = BasicBlock::Create(Ctx, "entry", OptFunc);
    BasicBlock* Continue = BasicBlock::Create(Ctx, "happy_path", OptFunc);
    BasicBlock* Unwind = BasicBlock::Create(Ctx, "unwind_path", OptFunc);

    // 4. The Hot Path
    Builder.SetInsertPoint(Entry);
    Value* MyVar = OptFunc->getArg(0);

    // ZERO-COST CALL: No 'icmp', no 'branch'.
    // If it throws, the OS/Runtime magically jumps to 'Unwind'.
    InvokeInst * Invoke = Builder.CreateInvoke(ThrowingFunc, Continue, Unwind, {});

    // 4. ATTACH THE METADATA
    // This is the magic line. We attach the "prof" (profile) metadata node.
    Invoke->setMetadata(LLVMContext::MD_prof, BranchWeights);
    Builder.SetInsertPoint(Continue);
    Builder.CreateRet(MyVar);

    // 5. The Cold Path (The Deoptimizing Landing Pad)
    Builder.SetInsertPoint(Unwind);
    
    // a. Mark this block as "Cold" (Help the CPU)
    // (In C++, you'd use metadata for this, effectively causing LLVM to move this block far away)
    
    // b. Catch the generic exception
    LandingPadInst* LP = Builder.CreateLandingPad(LPadRetTy, 1);
    LP->setCleanup(true); // Catch everything
    Value* ExObj = Builder.CreateExtractValue(LP, 0);

    // c. PACK STATE (The only logic allowed here)
    Value* Slot = Builder.CreateAlloca(SnapshotTy);
    Builder.CreateStore(ConstantInt::get(Int32Ty, 99), Builder.CreateStructGEP(SnapshotTy, Slot, 0)); // ID 99 = Exception
    Builder.CreateStore(MyVar, Builder.CreateStructGEP(SnapshotTy, Slot, 1)); // Save the variable
    Builder.CreateStore(ExObj, Builder.CreateStructGEP(SnapshotTy, Slot, 2)); // Save the exception object

    // d. BAIL OUT to the heavy logic
    Value* Res = Builder.CreateCall(FallbackFunc, { Slot });
    Builder.CreateRet(Res);
}
