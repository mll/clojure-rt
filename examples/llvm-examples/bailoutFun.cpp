void createInFlightDeopt(LLVMContext &Ctx, Module *M) {
    IRBuilder<> Builder(Ctx);
    Type* Int32Ty = Type::getInt32Ty(Ctx);

    // 1. The Full Snapshot Struct
    // Index 0: ResumeID
    // Index 1: Input X
    // Index 2: Intermediate Result from Step A (In-flight variable)
    std::vector<Type*> Fields = { Int32Ty, Int32Ty, Int32Ty };
    StructType* SnapshotTy = StructType::create(Ctx, Fields, "FullSnapshot");

    FunctionType* FT = FunctionType::get(Int32Ty, { SnapshotTy->getPointerTo() }, false);
    Function* Fallback = Function::Create(FT, Function::ExternalLinkage, "A.fallback", M);
    
    BasicBlock* Entry = BasicBlock::Create(Ctx, "entry", Fallback);
    BasicBlock* StepA = BasicBlock::Create(Ctx, "step_A", Fallback);
    BasicBlock* StepB = BasicBlock::Create(Ctx, "step_B", Fallback);
    
    // --- Entry: Unpack everything ---
    Builder.SetInsertPoint(Entry);
    Value* SnapshotPtr = Fallback->getArg(0);
    
    Value* ResumeID = Builder.CreateLoad(Int32Ty, Builder.CreateStructGEP(SnapshotTy, SnapshotPtr, 0), "rid");
    Value* X = Builder.CreateLoad(Int32Ty, Builder.CreateStructGEP(SnapshotTy, SnapshotPtr, 1), "x");
    
    // Load the in-flight variable. 
    // If we bail out at Step A, this value is garbage/ignored.
    // If we bail out at Step B, the optimized code filled this with Step A's result.
    Value* InFlightA = Builder.CreateLoad(Int32Ty, Builder.CreateStructGEP(SnapshotTy, SnapshotPtr, 2), "inflight_a");

    auto* Switch = Builder.CreateSwitch(ResumeID, StepA, 2);
    Switch->addCase(ConstantInt::get(Int32Ty, 0), StepA);
    Switch->addCase(ConstantInt::get(Int32Ty, 1), StepB);

    // --- Step A ---
    Builder.SetInsertPoint(StepA);
    // Logic: tmp = X * 10
    Value* ComputedA = Builder.CreateMul(X, ConstantInt::get(Int32Ty, 10), "computed_a");
    Builder.CreateBr(StepB);

    // --- Step B ---
    Builder.SetInsertPoint(StepB);
    
    // CRITICAL PART: We need "Result of A". 
    // We use a PHI node to choose between the value we just computed (if we came from Step A)
    // or the value from the struct (if we jumped here directly via Bailout 1).
    PHINode* PhiA = Builder.CreatePHI(Int32Ty, 2, "use_a");
    PhiA->addIncoming(ComputedA, StepA); // Came from Step A logic
    PhiA->addIncoming(InFlightA, Entry); // Jumped here via Switch/Entry

    Value* FinalRes = Builder.CreateAdd(PhiA, ConstantInt::get(Int32Ty, 5));
    Builder.CreateRet(FinalRes);
}
