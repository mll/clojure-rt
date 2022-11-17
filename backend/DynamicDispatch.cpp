#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::callStaticFun(const Node &node, const FnMethodNode &method, const string &name, const ObjectTypeSet &retValType, const vector<TypedValue> &args, const string &refName) {
  vector<Type *> argTypes;
  vector<Value *> argVals;
  vector<ObjectTypeSet> argT;
  
  for(auto arg : args) {
    argTypes.push_back((arg.first.isDetermined() && !arg.first.isBoxed) ? dynamicUnboxedType(arg.first.determinedType()) : dynamicBoxedType());
    argVals.push_back(arg.second);
    argT.push_back(arg.first);
  }

  string rName = recursiveMethodKey(name, argT);
  string rqName = fullyQualifiedMethodKey(name, argT, retValType);
  
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
  
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "failed", parentFunction);
  BasicBlock *functionTypeBB = llvm::BasicBlock::Create(*TheContext, "type_function", parentFunction);
  BasicBlock *vectorTypeBB = llvm::BasicBlock::Create(*TheContext, "type_vector", parentFunction);

  SwitchInst *cond = Builder->CreateSwitch(objectType, failedBB, 2); 
  cond->addCase(ConstantInt::get(*TheContext, APInt(8, functionType, false)), functionTypeBB);
  cond->addCase(ConstantInt::get(*TheContext, APInt(8, persistentVectorType, false)), vectorTypeBB);
 
  BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "merge", parentFunction);    
  
/* TODO - if the type is no longer a function, we still need to check type specific invokations.
    we cannot proceed to dynamic fun call because the new value may not be a fun. This needs to be added to the phi node as well. */ 
  
  Builder->SetInsertPoint(functionTypeBB);
  Value *funRetVal = nullptr;
  if(uniqueFunctionId) {
    BasicBlock *uniqueIdMergeBB = llvm::BasicBlock::Create(*TheContext, "merge", parentFunction);    
    BasicBlock *uniqueIdOkBB = llvm::BasicBlock::Create(*TheContext, "unique_id_ok", parentFunction);
    BasicBlock *uniqueIdFailedBB = llvm::BasicBlock::Create(*TheContext, "unique_id_failed", parentFunction);
    
    Value *uniqueIdPtr = Builder->CreateStructGEP(runtimeFunctionType(), objectToInvoke, 0, "get_unique_id");
    Value *uniqueId = Builder->CreateLoad(Type::getInt64Ty(*TheContext), uniqueIdPtr, "load_unique_id");    
        
    Value *cond2 = Builder->CreateICmpEQ(uniqueId, uniqueFunctionId, "cmp_unique_id");
    Builder->CreateCondBr(cond2, uniqueIdOkBB, uniqueIdFailedBB);
    
    Builder->SetInsertPoint(uniqueIdOkBB);
    vector<Value *> argVals;
    for(auto v : args) argVals.push_back(v.second);
    Value *uniqueIdOkVal = Builder->CreateCall(staticFunctionToCall, argVals, string("call_dynamic"));
    Builder->CreateBr(uniqueIdMergeBB);
    
    Builder->SetInsertPoint(uniqueIdFailedBB);
    Value *uniqueIdFailedVal = callDynamicFun(node, objectToInvoke, retValType, args);
    Builder->CreateBr(uniqueIdMergeBB);
    
    Builder->SetInsertPoint(uniqueIdMergeBB);
    PHINode *phiNode = Builder->CreatePHI(uniqueIdOkVal->getType(), 2, "phi");
    phiNode->addIncoming(uniqueIdOkVal, uniqueIdOkBB);
    phiNode->addIncoming(uniqueIdFailedVal, uniqueIdFailedBB);  
    funRetVal = phiNode;
  } else funRetVal = callDynamicFun(node, objectToInvoke, retValType, args);
  Builder->CreateBr(mergeBB);

  Builder->SetInsertPoint(vectorTypeBB);
  Value *vecRetVal = nullptr; 
  if(args.size() != 1) vecRetVal = runtimeException(CodeGenerationException("Wrong number of args passed to invokation", node)); 
  else {
    vector<TypedValue> finalArgs;
    string callName;
    finalArgs.push_back(TypedValue(ObjectTypeSet(persistentVectorType), objectToInvoke));
    auto argType = args[0].first;
    /* Todo - what about big integer? */
    if(argType.isDetermined() && !argType.isType(integerType)) vecRetVal = runtimeException(CodeGenerationException("The argument must be an integer", node)); 
    else {
      if(args[0].first.isType(integerType)) {
          finalArgs.push_back(args[0]);          
          callName = "PersistentVector_nth";
        } else {
          finalArgs.push_back(TypedValue(ObjectTypeSet::dynamicType(), args[0].second));          
          callName = "PersistentVector_dynamic_nth";
        }
        vecRetVal = callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs).second; 
    }
  }
  Builder->CreateBr(mergeBB);

  Builder->SetInsertPoint(failedBB);
  Value *failedRetVal = runtimeException(CodeGenerationException("This type cannot be invoked.", node));
  Builder->CreateBr(mergeBB);

  Builder->SetInsertPoint(mergeBB);
  PHINode *phiNode = Builder->CreatePHI(funRetVal->getType(), 3, "phi");
  phiNode->addIncoming(funRetVal, functionTypeBB);
  string type_str;
  llvm::raw_string_ostream rso(type_str);
  funRetVal->getType()->print(rso);
  vecRetVal->getType()->print(rso);
  std::cout<< "Types: " << rso.str() << endl;


  phiNode->addIncoming(vecRetVal, vectorTypeBB);  
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
       packed = ConstantInt::get(*TheContext, APInt(8, 0, false));
     }
     else {
       type = getRuntimeObjectType(arg.second);
       packed = ConstantInt::get(*TheContext, APInt(8, 1, false));
     }                                   
     argSignature = Builder->CreateShl(argSignature, 8, "shl");
     argSignature = Builder->CreateOr(argSignature, type, "bit_or");
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
       we should inspect it and decide if we want to use it or not for var inbvokations */ 

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
    
    auto result = codegen(method.body(), retVal);
    Builder->CreateRet((retVal.isBoxed || !retVal.isDetermined()) ? box(result).second : result.second);
    FunctionArgsStack.pop_back();
    
    verifyFunction(*CalleeF);
  }
}
