#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>

using namespace std;
using namespace llvm;

extern "C" {
  #include "runtime/Keyword.h"
}


/* Used by InvokeNode to compile a function during compile time:

     ExitOnErr(TheJIT->addAST(std::move(f)));
     
     and then insert just a direct call code to materialise it without any need of 
     dynamic arg discovery.
*/

TypedValue CodeGenerator::callStaticFun(const Node &node, const pair<FnMethodNode, uint64_t> &method, const string &name, const ObjectTypeSet &retValType, const vector<TypedValue> &args, const string &refName) {
  vector<Type *> argTypes;
  vector<Value *> argVals;
  vector<ObjectTypeSet> argT;
    
  for(auto arg : args) {
    argTypes.push_back(dynamicType(arg.first));
    argVals.push_back(arg.second);
    argT.push_back(arg.first);
  }

  string rName = ObjectTypeSet::recursiveMethodKey(name, argT);
  string rqName = ObjectTypeSet::fullyQualifiedMethodKey(name, argT, retValType);
  
  Type *retType = dynamicType(retValType);
  
  Function *CalleeF = TheModule->getFunction(rqName); 
  if(!CalleeF) {
    FunctionType *FT = FunctionType::get(retType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, rqName, TheModule.get());
  }

  auto f = make_unique<FunctionJIT>();
  f->methodIndex = method.second;
  f->args = argT;
  f->retVal = retValType;
  f->uniqueId = getUniqueFunctionIdFromName(name);
  
  ExitOnErr(TheJIT->addAST(std::move(f)));

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

/* 
   Builds code that dynamically determines what kind of action should be taken as 
   an invokation. It is possible we have some info on what should be called (acquired during compilation) - 
   in this case uniqueFunctionId and staticFunctionToCall are filled, and we use this info to simplify matters. 

*/

Value *CodeGenerator::dynamicInvoke(const Node &node, 
                                    Value *objectToInvoke, 
                                    Value* objectType, 
                                    const ObjectTypeSet &retValType, 
                                    const vector<TypedValue> &args, 
                                    Value *uniqueFunctionId, 
                                    Function *staticFunctionToCall) {
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
   
  parentFunction->insert(parentFunction->end(), functionTypeBB); 
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
    
    parentFunction->insert(parentFunction->end(), uniqueIdOkBB); 
    Builder->SetInsertPoint(uniqueIdOkBB);
    vector<Value *> argVals;
    for(auto v : args) argVals.push_back(v.second);
    Value *uniqueIdOkVal = Builder->CreateCall(staticFunctionToCall, argVals, string("call_dynamic"));
    Builder->CreateBr(uniqueIdMergeBB);
    
    parentFunction->insert(parentFunction->end(), uniqueIdFailedBB); 
    Builder->SetInsertPoint(uniqueIdFailedBB);
    Value *uniqueIdFailedVal = callDynamicFun(node, objectToInvoke, retValType, args);
    Builder->CreateBr(uniqueIdMergeBB);
    
    parentFunction->insert(parentFunction->end(), uniqueIdMergeBB); 
    Builder->SetInsertPoint(uniqueIdMergeBB);
    PHINode *phiNode = Builder->CreatePHI(uniqueIdOkVal->getType(), 2, "phi");
    phiNode->addIncoming(uniqueIdOkVal, uniqueIdOkBB);
    phiNode->addIncoming(uniqueIdFailedVal, uniqueIdFailedBB);  
    funRetVal = phiNode;
    funRetValBlock = Builder->GetInsertBlock();
  } else funRetVal = callDynamicFun(node, objectToInvoke, retValType, args);
  Builder->CreateBr(mergeBB);

  parentFunction->insert(parentFunction->end(), vectorTypeBB); 
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
    bool matched = false;

    if(args[0].first.isUnboxedType(integerType)) {
      finalArgs.push_back(args[0]);          
      callName = "PersistentVector_nth";
      matched = true;
    } 
    /* TODO - what about big integer? */
    if(args[0].first.isDynamic() || args[0].first.isBoxedType(integerType)) {
      finalArgs.push_back(TypedValue(ObjectTypeSet::dynamicType(), args[0].second));          
      callName = "PersistentVector_dynamic_nth";
      matched = true;
    }
    
    if(!matched) {
      runtimeException(CodeGenerationException("The argument must be an integer", node)); 
      vecRetVal = dynamicZero(retValType);
    } else {
      vecRetVal = callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs).second;       
      if(retValType.isScalar()) {
        auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), vecRetVal), retValType.determinedType());
        vecRetValBlock = p.first;
        vecRetVal = p.second;
      }
    }
  }

  Builder->CreateBr(mergeBB);

  parentFunction->insert(parentFunction->end(), persistentArrayMapTypeBB); 
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
    if(retValType.isScalar()) {
      auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), mapRetVal), retValType.determinedType());
      mapRetValBlock = p.first;
      mapRetVal = p.second;
    }
  }

  Builder->CreateBr(mergeBB);

  parentFunction->insert(parentFunction->end(), keywordTypeBB); 
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
    if(retValType.isScalar()) {
      auto p = dynamicUnbox(node, TypedValue(ObjectTypeSet::dynamicType(), keywordRetVal), retValType.determinedType());
      keywordRetValBlock = p.first;
      keywordRetVal = p.second;
    }
  }

  Builder->CreateBr(mergeBB);

  parentFunction->insert(parentFunction->end(), failedBB); 
  Builder->SetInsertPoint(failedBB);
  runtimeException(CodeGenerationException("This type cannot be invoked.", node));
  Value *failedRetVal = dynamicZero(retValType);

  Builder->CreateBr(mergeBB);
  parentFunction->insert(parentFunction->end(), mergeBB); 
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
  Value *argSignature[3] = { ConstantInt::get(*TheContext, APInt(64, 0, false)),
                             ConstantInt::get(*TheContext, APInt(64, 0, false)),
                             ConstantInt::get(*TheContext, APInt(64, 0, false)) } ;
  
  Value *packedArgSignature = ConstantInt::get(*TheContext, APInt(64, 0, false));

  vector<TypedValue> pointerCallArgs;

  if(args.size() > 20) throw CodeGenerationException("More than 20 args not yet supported", node); 
  for(int i=0; i<args.size(); i++) {
     auto arg = args[i];
     auto group = i / 8;
     auto index = i % 8;
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
     argSignature[group] = Builder->CreateShl(argSignature[group], 8, "shl");
     argSignature[group] = Builder->CreateOr(argSignature[group], Builder->CreateIntCast(type, Type::getInt64Ty(*TheContext), false), "bit_or");
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
  for (int i=0; i<3; i++) {
    specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), argSignature[i]));    
  }
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


ObjectTypeSet CodeGenerator::determineMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const vector<ObjectTypeSet> &args, const ObjectTypeSet &typeRestrictions) {

    string name = getMangledUniqueFunctionName(uniqueId);
    string rName = ObjectTypeSet::recursiveMethodKey(name, args);
    auto recursiveGuess = TheProgramme->RecursiveFunctionsRetValGuesses.find(rName);

    if(recursiveGuess != TheProgramme->RecursiveFunctionsRetValGuesses.end()) {
      return recursiveGuess->second;
    }

    auto recursiveName = TheProgramme->RecursiveFunctionsNameMap.find(rName);
    if(recursiveName != TheProgramme->RecursiveFunctionsNameMap.end()) {
      throw UnaccountedRecursiveFunctionEncounteredException(rName);
    }

    TheProgramme->RecursiveFunctionsNameMap.insert({rName, true});    
    
    unordered_map<string, ObjectTypeSet> namedArgs;

    // TODO: Variadic
    for(int i=0; i<method.params_size(); i++) {
      auto name = method.params(i).subnode().binding().name();
      namedArgs.insert({name, args[i]});
    }

    VariableBindingTypesStack.push_back(namedArgs);
    ObjectTypeSet retVal;
    bool found = false;
    bool foundSpecific = false;
    vector<CodeGenerationException> exceptions;
    try {
      retVal = getType(method.body(), typeRestrictions);
      if(!retVal.isEmpty()) found = true;
    } catch(UnaccountedRecursiveFunctionEncounteredException e) {
      if(e.functionName != rName) throw e;
      auto guesses = ObjectTypeSet::allGuesses();
      for(auto guess : guesses) {
        auto inserted = TheProgramme->RecursiveFunctionsRetValGuesses.insert({rName, guess});
        inserted.first->second = guess;
        try {
          retVal = getType(method.body(), typeRestrictions).unboxed();
          if(!retVal.isEmpty()) found = true;
          if(retVal.isDetermined()) foundSpecific = true;
        } catch(CodeGenerationException e) {
          exceptions.push_back(e);
        }
        if(foundSpecific) break;
      }
    }
    VariableBindingTypesStack.pop_back();

    TheProgramme->RecursiveFunctionsNameMap.erase(rName);
    TheProgramme->RecursiveFunctionsRetValGuesses.erase(rName);
    // TODO: better error here, maybe use the exceptions vector? 
    if(!found) {
      for(auto e : exceptions) cout << e.toString() << endl;
      throw CodeGenerationException("Unable to create function with given params", method.body());
    }
    return retVal;
} 


void CodeGenerator::buildStaticFun(const int64_t uniqueId, const uint64_t methodIndex, const string &name, const ObjectTypeSet &retType, const vector<ObjectTypeSet> &args) {

  const FnNode &node = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();
  const FnMethodNode &method = node.methods(methodIndex).subnode().fnmethod();
  
  string rName = ObjectTypeSet::fullyQualifiedMethodKey(name, args, retType);
  const ObjectTypeSet realRetType = determineMethodReturn(method, uniqueId, args, ObjectTypeSet::all());
  
  vector<Type *> argTypes;
  for(auto arg : args) {
    argTypes.push_back(dynamicType(arg));
  }
 
  Type *retFunType = dynamicType(retType);

  Function *CalleeF = TheModule->getFunction(rName); 
  if(!CalleeF) {
    FunctionType *FT = FunctionType::get(retFunType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, rName, TheModule.get());
    /* Build body */
    vector<Value *> fArgs;
    for(auto &farg: CalleeF->args()) fArgs.push_back(&farg);

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
    Builder->SetInsertPoint(BB);

    unordered_map<string, ObjectTypeSet> functionArgTypes;
    unordered_map<string, TypedValue> namedFunctionArgs;
    vector<TypedValue> functionArgs;
    // TODO: Variadic
    for(int i=0; i<method.params_size(); i++) {
      auto name = method.params(i).subnode().binding().name();
      auto value = TypedValue(args[i].removeConst(), fArgs[i]);
      functionArgTypes.insert({name, args[i].removeConst()});      
      namedFunctionArgs.insert({name, value});
      functionArgs.push_back(value);
    }
    
    VariableBindingTypesStack.push_back(functionArgTypes);
    VariableBindingStack.push_back(namedFunctionArgs);
    try {
      if(realRetType == retType || (!retType.isDetermined() && !realRetType.isDetermined())) {
        auto result = codegen(method.body(), retType);
        Builder->CreateRet(retType.isBoxedScalar() ? box(result).second : result.second);
      } else {
        auto f = make_unique<FunctionJIT>();
        f->methodIndex = methodIndex;
        f->args = args;
        f->retVal = realRetType;
        f->uniqueId = uniqueId;
        ExitOnErr(TheJIT->addAST(std::move(f)));
        
        TypedValue realRet = callRuntimeFun(ObjectTypeSet::fullyQualifiedMethodKey(name, args, realRetType), realRetType, functionArgs);
        Value *finalRet = nullptr;
        if(realRetType.isDynamic() && retType.isScalar()) 
          finalRet = dynamicUnbox(method.body(), realRet, retType.determinedType()).second;
        if(realRetType.isScalar() && retType.isDynamic()) 
          finalRet = box(realRet).second;
        
        if(realRetType.isDetermined() && retType.isDetermined()) {
          if(realRetType.determinedType() != retType.determinedType()) throw CodeGenerationException("Types mismatch for function call: " + ObjectTypeSet::typeStringForArg(retType) + " " + ObjectTypeSet::typeStringForArg(realRetType), method.body());
          
          if(realRetType.isBoxedScalar()) finalRet = dynamicUnbox(method.body(), realRet, retType.determinedType()).second;
          if(retType.isBoxedScalar()) finalRet = box(realRet).second;
        }

        if(finalRet == nullptr) finalRet = realRet.second;
        Builder->CreateRet(finalRet);
      }
      verifyFunction(*CalleeF);
    } catch (CodeGenerationException e) {
      CalleeF->eraseFromParent();
      FunctionType *FT = FunctionType::get(retFunType, argTypes, false);
      CalleeF = Function::Create(FT, Function::ExternalLinkage, rName, TheModule.get());
      BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
      Builder->SetInsertPoint(BB);
      runtimeException(e);  
      Builder->CreateRet(dynamicZero(retType));
      verifyFunction(*CalleeF);
    }
    VariableBindingStack.pop_back();
    VariableBindingTypesStack.pop_back();
  }
}
