#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>

using namespace std;
using namespace llvm;

extern "C" {
  #include "runtime/Keyword.h"
}


TypedValue CodeGenerator::callStaticFun(const Node &node, const FnMethodNode &method, const string &name, const ObjectTypeSet &retValType, const vector<TypedValue> &args, const string &refName) {
  vector<Type *> argTypes;
  vector<Value *> argVals;
  vector<ObjectTypeSet> argT;
    
  for(auto arg : args) {
    argTypes.push_back((arg.first.isDetermined() && !arg.first.isBoxed) ? dynamicUnboxedType(arg.first.determinedType()) : dynamicBoxedType());
    argVals.push_back(arg.second);
    argT.push_back(arg.first);
  }

  string rName = ObjectTypeSet::recursiveMethodKey(name, argT);
  string rqName = ObjectTypeSet::fullyQualifiedMethodKey(name, argT, retValType);
  
  Type *retType = retValType.isDetermined() ? dynamicUnboxedType(retValType.determinedType()) : dynamicBoxedType();
  
  Function *CalleeF = TheModule->getFunction(rqName); 
  if(!CalleeF) {
    FunctionType *FT = FunctionType::get(retType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, rqName, TheModule.get());
  }

  auto f = make_unique<FunctionJIT>();
  f->method = method;
  f->args = argT;
  f->retVal = retValType;
  f->name = rqName;
  
  ExitOnErr(TheJIT->addAST(move(f)));

  auto found = StaticVars.find(refName);
  if(found != StaticVars.end()) {
    LoadInst * load = Builder->CreateLoad(dynamicBoxedType(), found->second.second, "load_var");
    load->setAtomic(AtomicOrdering::Monotonic);
    Value *type = getRuntimeObjectType(load);
    Value *uniqueId = ConstantInt::get(*TheContext, APInt(64, getUniqueFunctionIdFromName(name), false));   
    return TypedValue(retValType, dynamicInvoke(node, load, type, retValType, args, uniqueId, CalleeF));  
  }

  return TypedValue(retValType, Builder->CreateCall(CalleeF, argVals, string("call_") + rName));  
}

Value *CodeGenerator::dynamicInvoke(const Node &node, Value *objectToInvoke, Value* objectType, const ObjectTypeSet &retValType, const vector<TypedValue> &args, Value *uniqueFunctionId, Function *staticFunctionToCall) {
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  
  BasicBlock *functionTypeBB = llvm::BasicBlock::Create(*TheContext, "type_function");
  BasicBlock *vectorTypeBB = llvm::BasicBlock::Create(*TheContext, "type_vector");
  BasicBlock *persistentArrayMapTypeBB = llvm::BasicBlock::Create(*TheContext, "type_persistentArrayMap");
  BasicBlock *keywordTypeBB = llvm::BasicBlock::Create(*TheContext, "type_keyword");
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "failed");

  SwitchInst *cond = Builder->CreateSwitch(objectType, failedBB, 2); 
  cond->addCase(ConstantInt::get(*TheContext, APInt(32, functionType, false)), functionTypeBB);
  cond->addCase(ConstantInt::get(*TheContext, APInt(32, persistentVectorType, false)), vectorTypeBB);
  cond->addCase(ConstantInt::get(*TheContext, APInt(32, persistentArrayMapType, false)), persistentArrayMapTypeBB);
  cond->addCase(ConstantInt::get(*TheContext, APInt(32, keywordType, false)), keywordTypeBB);
 
  BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "merge");    
   
  parentFunction->getBasicBlockList().push_back(functionTypeBB); 
  Builder->SetInsertPoint(functionTypeBB);

  Value *funRetVal = nullptr;
  BasicBlock *funRetValBlock = Builder->GetInsertBlock();
  if(uniqueFunctionId) {
    BasicBlock *uniqueIdMergeBB = llvm::BasicBlock::Create(*TheContext, "unique_id_merge");    
    BasicBlock *uniqueIdOkBB = llvm::BasicBlock::Create(*TheContext, "unique_id_ok");
    BasicBlock *uniqueIdFailedBB = llvm::BasicBlock::Create(*TheContext, "unique_id_failed");
    
    Value *uniqueIdPtr = Builder->CreateStructGEP(runtimeFunctionType(), objectToInvoke, 0, "get_unique_id");
    Value *uniqueId = Builder->CreateLoad(Type::getInt64Ty(*TheContext), uniqueIdPtr, "load_unique_id");    
        
    Value *cond2 = Builder->CreateICmpEQ(uniqueId, uniqueFunctionId, "cmp_unique_id");
    Builder->CreateCondBr(cond2, uniqueIdOkBB, uniqueIdFailedBB);
    
    parentFunction->getBasicBlockList().push_back(uniqueIdOkBB); 
    Builder->SetInsertPoint(uniqueIdOkBB);
    vector<Value *> argVals;
    for(auto v : args) argVals.push_back(v.second);
    Value *uniqueIdOkVal = Builder->CreateCall(staticFunctionToCall, argVals, string("call_dynamic"));
    Builder->CreateBr(uniqueIdMergeBB);
    
    parentFunction->getBasicBlockList().push_back(uniqueIdFailedBB); 
    Builder->SetInsertPoint(uniqueIdFailedBB);
    Value *uniqueIdFailedVal = callDynamicFun(node, objectToInvoke, retValType, args);
    Builder->CreateBr(uniqueIdMergeBB);
    
    parentFunction->getBasicBlockList().push_back(uniqueIdMergeBB); 
    Builder->SetInsertPoint(uniqueIdMergeBB);
    PHINode *phiNode = Builder->CreatePHI(uniqueIdOkVal->getType(), 2, "phi");
    phiNode->addIncoming(uniqueIdOkVal, uniqueIdOkBB);
    phiNode->addIncoming(uniqueIdFailedVal, uniqueIdFailedBB);  
    funRetVal = phiNode;
    funRetValBlock = Builder->GetInsertBlock();
  } else funRetVal = callDynamicFun(node, objectToInvoke, retValType, args);
  Builder->CreateBr(mergeBB);

  parentFunction->getBasicBlockList().push_back(vectorTypeBB); 
  Builder->SetInsertPoint(vectorTypeBB);
  Value *vecRetVal = nullptr; 
  BasicBlock *vecRetValBlock = Builder->GetInsertBlock();
  if(args.size() != 1) {
    runtimeException(CodeGenerationException("Wrong number of args passed to invokation", node)); 
    vecRetVal = dynamicZero(retValType);
  }
  else {
    vector<TypedValue> finalArgs;
    string callName;
    finalArgs.push_back(TypedValue(ObjectTypeSet(persistentVectorType), objectToInvoke));
    auto argType = args[0].first;
    /* Todo - what about big integer? */
    if(argType.isDetermined() && !argType.isType(integerType)) {
      runtimeException(CodeGenerationException("The argument must be an integer", node)); 
      vecRetVal = dynamicZero(retValType);
    } else {
      if(args[0].first.isType(integerType)) {
          finalArgs.push_back(args[0]);          
          callName = "PersistentVector_nth";
        } else {
          finalArgs.push_back(TypedValue(ObjectTypeSet::dynamicType(), args[0].second));          
          callName = "PersistentVector_dynamic_nth";
        }
        vecRetVal = callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs).second; 
        if(!retValType.isBoxed && retValType.isDetermined()) {
          auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), vecRetVal), retValType.determinedType());
          vecRetValBlock = p.first;
          vecRetVal = p.second;
        }
    }
  }

  Builder->CreateBr(mergeBB);

  parentFunction->getBasicBlockList().push_back(persistentArrayMapTypeBB); 
  Builder->SetInsertPoint(persistentArrayMapTypeBB);
  Value *mapRetVal = nullptr; 
  BasicBlock *mapRetValBlock = Builder->GetInsertBlock();
  if(args.size() != 1) {
    runtimeException(CodeGenerationException("Wrong number of args passed to invokation", node)); 
    mapRetVal = dynamicZero(retValType);
  }
  else {
    vector<TypedValue> finalArgs;
    string callName;
    finalArgs.push_back(TypedValue(ObjectTypeSet(persistentArrayMapType), objectToInvoke));
    auto argType = args[0].first;

    finalArgs.push_back(box(args[0]));          
    callName = "PersistentArrayMap_get";
  
    mapRetVal = callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs).second; 
    if(!retValType.isBoxed && retValType.isDetermined()) {
      auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), mapRetVal), retValType.determinedType());
      mapRetValBlock = p.first;
      mapRetVal = p.second;
    }
  }

  Builder->CreateBr(mergeBB);

  parentFunction->getBasicBlockList().push_back(keywordTypeBB); 
  Builder->SetInsertPoint(keywordTypeBB);
  Value *keywordRetVal = nullptr; 
  BasicBlock *keywordRetValBlock = Builder->GetInsertBlock();
  if(args.size() != 1) {
    runtimeException(CodeGenerationException("Wrong number of args passed to invokation", node)); 
    keywordRetVal = dynamicZero(retValType);
  }
  else {
    vector<TypedValue> finalArgs;
    string callName;
    finalArgs.push_back(box(args[0]));          
    finalArgs.push_back(TypedValue(ObjectTypeSet(keywordType), objectToInvoke));
    auto argType = args[0].first;

    callName = "PersistentArrayMap_dynamic_get";
  
    keywordRetVal = callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs).second; 
    if(!retValType.isBoxed && retValType.isDetermined()) {
      auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), keywordRetVal), retValType.determinedType());
      keywordRetValBlock = p.first;
      keywordRetVal = p.second;
    }
  }

  Builder->CreateBr(mergeBB);



  parentFunction->getBasicBlockList().push_back(failedBB); 
  Builder->SetInsertPoint(failedBB);
  runtimeException(CodeGenerationException("This type cannot be invoked.", node));
  Value *failedRetVal = dynamicZero(retValType);

  Builder->CreateBr(mergeBB);
  parentFunction->getBasicBlockList().push_back(mergeBB); 
  Builder->SetInsertPoint(mergeBB);
  PHINode *phiNode = Builder->CreatePHI(funRetVal->getType(), 5, "phi");

  phiNode->addIncoming(funRetVal, funRetValBlock);
  phiNode->addIncoming(vecRetVal, vecRetValBlock);  
  phiNode->addIncoming(mapRetVal, mapRetValBlock);  
  phiNode->addIncoming(keywordRetVal, keywordRetValBlock);  
  phiNode->addIncoming(failedRetVal, failedBB);  
  return phiNode;
}


Value *CodeGenerator::callDynamicFun(const Node &node, Value *rtFnPointer, const ObjectTypeSet &retValType, const vector<TypedValue> &args) {
  Value *argSignature = ConstantInt::get(*TheContext, APInt(64, 0, false));
  Value *packedArgSignature = ConstantInt::get(*TheContext, APInt(64, 0, false));
  vector<TypedValue> pointerCallArgs;
  /* TODO - more than 8 args */
  if(args.size() > 8) throw CodeGenerationException("More than 8 args not yet supported", node); 
  for(int i=0; i<args.size(); i++) {
     auto arg = args[i];
     Value *type = nullptr;
     Value *packed = nullptr;
     if(arg.first.isDetermined()) {
       type = ConstantInt::get(*TheContext, APInt(64, arg.first.determinedType(), false));
       packed = ConstantInt::get(*TheContext, APInt(64, 0, false));
     }
     else {
       type = getRuntimeObjectType(arg.second);
       packed = ConstantInt::get(*TheContext, APInt(64, 1, false));
     }                                   
     argSignature = Builder->CreateShl(argSignature, 8, "shl");
     argSignature = Builder->CreateOr(argSignature, Builder->CreateIntCast(type, Type::getInt64Ty(*TheContext), false), "bit_or");
     packedArgSignature = Builder->CreateShl(packedArgSignature, 1, "shl");
     packedArgSignature = Builder->CreateOr(packedArgSignature, packed, "bit_or");
  }
  vector<TypedValue> specialisationArgs;

  Value* jitAddressConst = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (int64_t)TheJIT, false));
  Value* jitAddress = Builder->CreateIntToPtr(jitAddressConst, Type::getInt8Ty(*TheContext)->getPointerTo(), "int_to_ptr"); 

  specialisationArgs.push_back(TypedValue(ObjectTypeSet::all(), jitAddress));
  specialisationArgs.push_back(TypedValue(ObjectTypeSet::all(), rtFnPointer));
  specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), ConstantInt::get(*TheContext, APInt(64, retValType.isDetermined() ? retValType.determinedType() : 0, false))));
  specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), ConstantInt::get(*TheContext, APInt(64, args.size(), false))));
  specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), argSignature));
  specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), packedArgSignature));

  TypedValue functionPointer = callRuntimeFun("specialiseDynamicFn", ObjectTypeSet::all(), specialisationArgs);

  vector<Type *> argTypes;
  vector<Value *> argVals;
  for(auto arg: args) { 
    argTypes.push_back(dynamicType(arg.first));
    argVals.push_back(arg.second);
  }

  auto retVal = dynamicType(retValType);
    /* TODO - Return values should be probably handled differently. e.g. if the new function returns something boxed, 
       we should inspect it and decide if we want to use it or not for var invokations */ 

  /* What if the new function returns a different type? we should probably not force the return type. It should be deduced by jit and our job should be to somehow box/unbox it. Alternatively, we can add return type to method name (like fn_1_JJ_LV) and force generation of function that returns the boxed version of needed return type. If it does not match - we should throw a runtime error */ 

  FunctionType *FT = FunctionType::get(retVal, argTypes, false);
  Value *callablePointer = Builder->CreatePointerCast(functionPointer.second, FT->getPointerTo());
  
  return Builder->CreateCall(FunctionCallee(FT, callablePointer), argVals, string("call_dynamic"));
}

void CodeGenerator::buildStaticFun(const FnMethodNode &method, const string &name, const ObjectTypeSet &retVal, const vector<ObjectTypeSet> &args) {
  vector<Type *> argTypes;
  for(auto arg : args) {
    argTypes.push_back((arg.isDetermined() && !arg.isBoxed)? dynamicUnboxedType(arg.determinedType()) : dynamicBoxedType());
  }
 
  Type *retType = (retVal.isDetermined() && !retVal.isBoxed) ? dynamicUnboxedType(retVal.determinedType()) : dynamicBoxedType();
  
  Function *CalleeF = TheModule->getFunction(name); 
  if(!CalleeF) {
    FunctionType *FT = FunctionType::get(retType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, name, TheModule.get());
    /* Build body */
    vector<Value *> fArgs;
    for(auto &farg: CalleeF->args()) fArgs.push_back(&farg);

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
    Builder->SetInsertPoint(BB);
    
    vector<TypedValue> functionArgs;
    for(int i=0; i<args.size(); i++) functionArgs.push_back(unbox(TypedValue(args[i].removeConst(), fArgs[i])));
    FunctionArgsStack.push_back(functionArgs);
    try {
      auto result = codegen(method.body(), retVal);
      Builder->CreateRet((retVal.isBoxed || !retVal.isDetermined()) ? box(result).second : result.second);
      verifyFunction(*CalleeF);
    } catch (CodeGenerationException e) {
      CalleeF->eraseFromParent();
      FunctionType *FT = FunctionType::get(retType, argTypes, false);
      CalleeF = Function::Create(FT, Function::ExternalLinkage, name, TheModule.get());
      BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
      Builder->SetInsertPoint(BB);
      runtimeException(e);  
      Builder->CreateRet(dynamicZero(retVal));
      verifyFunction(*CalleeF);
    }
    FunctionArgsStack.pop_back();
  }
}
