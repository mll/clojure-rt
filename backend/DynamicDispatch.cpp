#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "cljassert.h"

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

TypedValue CodeGenerator::callStaticFun(const Node &node, 
                                        const FnNode& body, 
                                        const pair<FnMethodNode, uint64_t> &method, 
                                        const string &name, 
                                        const ObjectTypeSet &retValType, 
                                        const std::vector<TypedValue> &args, 
                                        const string &refName, 
                                        const TypedValue &callObject, 
                                        std::vector<ObjectTypeSet> &closedOverTypes) {
  vector<Type *> argTypes;
  vector<Value *> argVals;
  vector<ObjectTypeSet> argT;

  vector<TypedValue> finalArgs;

  CLJ_ASSERT(method.first.fixedarity() <= args.size(), "Wrong number of params: received " + to_string(args.size()) + " required " + to_string(method.first.fixedarity()));

  for(int i=0; i<method.first.fixedarity(); i++) {
    auto arg = args[i];
    argTypes.push_back(dynamicType(arg.first));
    argVals.push_back(arg.second);
    argT.push_back(arg.first);
    finalArgs.push_back(arg);
  }
 
  // TODO: For now we just use a vector, in the future a faster sequable data structure will be used here */
  if(method.first.isvariadic()) {
    auto type = ObjectTypeSet(persistentVectorType);
    vector<TypedValue> vecArgs(args.begin() + method.first.fixedarity(), args.end()); 
    argTypes.push_back(dynamicType(type));
    argT.push_back(type);
    auto v = dynamicVector(vecArgs);
    
    argVals.push_back(v);
    finalArgs.push_back(TypedValue(type, v));
  }

//  cout << "Calling static " << argT.size() << endl;


  /* Add closed overs */
  argTypes.push_back(dynamicType(callObject.first));
  argVals.push_back(callObject.second);
  
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
  f->closedOvers = closedOverTypes;
  
  ExitOnErr(TheJIT->addAST(std::move(f)));

  auto retVal = TypedValue(retValType, Builder->CreateCall(CalleeF, argVals, string("call_") + rName));  
  if(body.once()) {
    vector<TypedValue> cleanupArgs;
    cleanupArgs.push_back(callObject);    
    callRuntimeFun("Function_cleanupOnce", ObjectTypeSet::empty(), cleanupArgs);       
  }

  return retVal;
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
  //cout << "Calling dynamic " << args.size() << endl;
  BasicBlock *initialBB = Builder->GetInsertBlock();
  Function *parentFunction = initialBB->getParent();
  
  BasicBlock *varTypeBB = llvm::BasicBlock::Create(*TheContext, "type_var", parentFunction);
  BasicBlock *derefedBB = llvm::BasicBlock::Create(*TheContext, "derefed", parentFunction);
  BasicBlock *functionTypeBB = llvm::BasicBlock::Create(*TheContext, "type_function");
  BasicBlock *vectorTypeBB = llvm::BasicBlock::Create(*TheContext, "type_vector");
  BasicBlock *persistentArrayMapTypeBB = llvm::BasicBlock::Create(*TheContext, "type_persistentArrayMap");
  BasicBlock *keywordTypeBB = llvm::BasicBlock::Create(*TheContext, "type_keyword");
  BasicBlock *failedBB = llvm::BasicBlock::Create(*TheContext, "failed");

  // Calling var is the same as calling whatever is inside the var, deref if necessary
  Value *isVar = Builder->CreateICmpEQ(objectType, ConstantInt::get(*TheContext, APInt(32, varType, false)), "cmp_var_type");
  Builder->CreateCondBr(isVar, varTypeBB, derefedBB);
  
  Builder->SetInsertPoint(varTypeBB);
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  Value *derefedVar = callRuntimeFun("Var_deref", ptrT, {ptrT}, {objectToInvoke});
  Value *derefedType = getRuntimeObjectType(derefedVar);
  Builder->CreateBr(derefedBB);

  Builder->SetInsertPoint(derefedBB);
  PHINode *actualObjectType = Builder->CreatePHI(objectType->getType(), 2, "object_type");
  actualObjectType->addIncoming(derefedType, varTypeBB);
  actualObjectType->addIncoming(objectType, initialBB);
  PHINode *actualObjectToInvoke = Builder->CreatePHI(objectToInvoke->getType(), 2, "object_to_invoke");
  actualObjectToInvoke->addIncoming(derefedVar, varTypeBB);
  actualObjectToInvoke->addIncoming(objectToInvoke, initialBB);
  SwitchInst *cond = Builder->CreateSwitch(actualObjectType, failedBB, 2); 
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
   // cout << "Unique " << endl;
    BasicBlock *uniqueIdMergeBB = llvm::BasicBlock::Create(*TheContext, "unique_id_merge");    
    BasicBlock *uniqueIdOkBB = llvm::BasicBlock::Create(*TheContext, "unique_id_ok");
    BasicBlock *uniqueIdFailedBB = llvm::BasicBlock::Create(*TheContext, "unique_id_failed");
    
    Value *uniqueIdPtr = Builder->CreateStructGEP(runtimeFunctionType(), actualObjectToInvoke, 3, "get_unique_id");
    Value *uniqueId = Builder->CreateLoad(Type::getInt64Ty(*TheContext), uniqueIdPtr, "load_unique_id");    
        
    Value *cond2 = Builder->CreateICmpEQ(uniqueId, uniqueFunctionId, "cmp_unique_id");
    Builder->CreateCondBr(cond2, uniqueIdOkBB, uniqueIdFailedBB);
    
    parentFunction->insert(parentFunction->end(), uniqueIdOkBB); 
    Builder->SetInsertPoint(uniqueIdOkBB);
    vector<Value *> argVals;
    for(auto v : args) argVals.push_back(v.second);
    /* The last arg is always function object, so that we can extract the closed overs inside the function */
    argVals.push_back(actualObjectToInvoke);

    Value *uniqueIdOkVal = Builder->CreateCall(staticFunctionToCall, argVals, string("call_dynamic"));
    
    Value *oncePtr = Builder->CreateStructGEP(runtimeFunctionType(), actualObjectToInvoke, 1, "get_once");
    Value *once = Builder->CreateLoad(Type::getInt8Ty(*TheContext), oncePtr, "load_once");
    Value *onceCond = Builder->CreateIntCast(once, dynamicUnboxedType(booleanType), false);
    
    BasicBlock *cleanupBB = llvm::BasicBlock::Create(*TheContext, "cleanup", parentFunction);
    BasicBlock *ccBB = llvm::BasicBlock::Create(*TheContext, "ccBB", parentFunction);

    Builder->CreateCondBr(onceCond, cleanupBB, ccBB);
    Builder->SetInsertPoint(cleanupBB);

    vector<TypedValue> cleanupArgs;
    cleanupArgs.push_back(TypedValue(ObjectTypeSet(functionType), actualObjectToInvoke));    
    callRuntimeFun("Function_cleanupOnce", ObjectTypeSet::empty(), cleanupArgs);       

    Builder->CreateBr(ccBB);
    Builder->SetInsertPoint(ccBB);
    Builder->CreateBr(uniqueIdMergeBB);

  
    parentFunction->insert(parentFunction->end(), uniqueIdFailedBB); 
    Builder->SetInsertPoint(uniqueIdFailedBB);
    Value *uniqueIdFailedVal = callDynamicFun(node, actualObjectToInvoke, retValType, args);
    uniqueIdFailedBB = Builder->GetInsertBlock();

    Builder->CreateBr(uniqueIdMergeBB);
    
    parentFunction->insert(parentFunction->end(), uniqueIdMergeBB); 
    Builder->SetInsertPoint(uniqueIdMergeBB);
    PHINode *phiNode = Builder->CreatePHI(uniqueIdOkVal->getType(), 2, "phi");
    phiNode->addIncoming(uniqueIdOkVal, ccBB);
    phiNode->addIncoming(uniqueIdFailedVal, uniqueIdFailedBB);  
    funRetVal = phiNode;
    funRetValBlock = Builder->GetInsertBlock();
  } else { 
    funRetVal = callDynamicFun(node, actualObjectToInvoke, retValType, args); 
    funRetValBlock = Builder->GetInsertBlock();
  }
  
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
    finalArgs.push_back(TypedValue(ObjectTypeSet(persistentVectorType), actualObjectToInvoke));
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
    finalArgs.push_back(TypedValue(ObjectTypeSet(persistentArrayMapType), actualObjectToInvoke));
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
    finalArgs.push_back(TypedValue(ObjectTypeSet(keywordType), actualObjectToInvoke));
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
  /* 21 because varaidic adds one arg that packs the remaining */
  if(args.size() > 21) throw CodeGenerationException("More than 20 args not supported", node); 
  for(int i=0; i<args.size(); i++) {
     auto arg = args[i];
     auto group = i / 8;
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
  /* Last arg is a pointer to the function object, so that we can extract closed overs */
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  argVals.push_back(rtFnPointer);

  auto retVal = dynamicType(retValType);
    /* TODO - Return values should be probably handled differently. e.g. if the new function returns something boxed, 
       we should inspect it and decide if we want to use it or not for var invokations */ 

  /* What if the new function returns a different type? we should probably not force the return type. It should be deduced by jit and our job should be to somehow box/unbox it. Alternatively, we can add return type to method name (like fn_1_JJ_LV) and force generation of function that returns the boxed version of needed return type. If it does not match - we should throw a runtime error */ 

  FunctionType *FT = FunctionType::get(retVal, argTypes, false);
  Value *callablePointer = Builder->CreatePointerCast(functionPointer.second, FT->getPointerTo());
  auto finalRetVal = Builder->CreateCall(FunctionCallee(FT, callablePointer), argVals, string("call_dynamic"));

  Function *parentFunction = Builder->GetInsertBlock()->getParent();  
  Value *oncePtr = Builder->CreateStructGEP(runtimeFunctionType(), rtFnPointer, 1, "get_once");
  Value *once = Builder->CreateLoad(Type::getInt8Ty(*TheContext), oncePtr, "load_once");
  Value *onceCond = Builder->CreateIntCast(once, dynamicUnboxedType(booleanType), false);
    
  BasicBlock *cleanupBB = llvm::BasicBlock::Create(*TheContext, "cleanup", parentFunction);
  BasicBlock *continueBB = llvm::BasicBlock::Create(*TheContext, "continue", parentFunction);

  Builder->CreateCondBr(onceCond, cleanupBB, continueBB);
  Builder->SetInsertPoint(cleanupBB);

  vector<TypedValue> cleanupArgs;
  cleanupArgs.push_back(TypedValue(ObjectTypeSet(functionType), rtFnPointer));    
  callRuntimeFun("Function_cleanupOnce", ObjectTypeSet::empty(), cleanupArgs);       
  Builder->CreateBr(continueBB);
  Builder->SetInsertPoint(continueBB);
  
  return finalRetVal;
}


ObjectTypeSet CodeGenerator::determineMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions) {
  string name = getMangledUniqueFunctionName(uniqueId);
  string rName = ObjectTypeSet::recursiveMethodKey(name, args);
  auto functionInference = TheProgramme->FunctionsRetValInfers.find(rName);

  if (functionInference == TheProgramme->FunctionsRetValInfers.end()) {
    TheProgramme->FunctionsRetValInfers.insert({rName, ObjectTypeSet()});
  } else {
    return functionInference->second;
  }
  
  auto returnType = ObjectTypeSet(), newReturnType = ObjectTypeSet();
  do {
    returnType = newReturnType;
    newReturnType = inferMethodReturn(method, uniqueId, args, closedOvers, typeRestrictions);
    TheProgramme->FunctionsRetValInfers.insert_or_assign(rName, newReturnType);
  } while (!(returnType == newReturnType));
  
  return newReturnType;
}

ObjectTypeSet CodeGenerator::inferMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions) {
    unordered_map<string, ObjectTypeSet> namedArgs;

    for(int i=0; i<method.fixedarity(); i++) {
      auto name = method.params(i).subnode().binding().name();
      namedArgs.insert({name, args[i]});
    }

    if (method.isvariadic()) {
      string name;
      if (method.params_size() == method.fixedarity()) name = "***unbound-variadic***";
      else name = method.params(method.fixedarity()).subnode().binding().name();
      namedArgs.insert({name, ObjectTypeSet(persistentVectorType)});
    }
    
    for(int i=0; i<method.closedovers_size(); i++) {
      auto name = method.closedovers(i).subnode().local().name();
      if(namedArgs.find(name) == namedArgs.end()) {
        namedArgs.insert({name, closedOvers[i].unboxed()});  
      }
    }

    VariableBindingTypesStack.push_back(namedArgs);
    ObjectTypeSet retVal;
    vector<CodeGenerationException> exceptions;
    try {
      retVal = getType(method.body(), typeRestrictions);
    } catch(CodeGenerationException e) {
      exceptions.push_back(e);
    }

    VariableBindingTypesStack.pop_back();

    // TODO: better error here, maybe use the exceptions vector? 
    if(retVal.isEmpty()) {
      std::string exceptionString;
      for(auto e : exceptions) exceptionString += e.toString() + "\n";
      throw CodeGenerationException("Unable to create function with given params: "+ exceptionString , method.body());
    }
    return retVal;
} 


/* Called by JIT to build the body of a function. At this stage all arg types are determined */

void CodeGenerator::buildStaticFun(const int64_t uniqueId, 
                                   const uint64_t methodIndex, 
                                   const string &name, 
                                   const ObjectTypeSet &retType, 
                                   const std::vector<ObjectTypeSet> &args, 
                                   std::vector<ObjectTypeSet> &closedOvers) {
  const FnNode &node = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();
  const FnMethodNode &method = node.methods(methodIndex).subnode().fnmethod();
  // cout << "BUILD STATIC " << endl;
  // cout << method.fixedarity() << endl;
  // cout << args.size() << endl;
  // cout << args.back().toString() << endl;
  CLJ_ASSERT(method.params_size() <= args.size(), "Wrong number of params: received " + to_string(args.size()) + " required " + to_string(method.params_size()));

  string rName = ObjectTypeSet::fullyQualifiedMethodKey(name, args, retType);

  vector<Type *> argTypes;
  for(auto arg : args) {
    argTypes.push_back(dynamicType(arg));
  }
  /* Last arg is a pointer to the function object, so that we can extract closed overs */
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());

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
    
    const ObjectTypeSet realRetType = determineMethodReturn(method, uniqueId, args, closedOvers, ObjectTypeSet::all());

    if(realRetType == retType || (!retType.isDetermined() && !realRetType.isDetermined())) {      
      for(int i=0; i<method.params_size(); i++) {
        auto name = method.params(i).subnode().binding().name();
        auto value = TypedValue(args[i].removeConst(), fArgs[i]);
        functionArgTypes.insert({name, args[i].removeConst()});      
        namedFunctionArgs.insert({name, value});
      }
      
      Value *functionObject = fArgs[method.params_size()];
      
      if (method.closedovers_size() > 0) {        
        vector<Value *> gepAccess;
        gepAccess.push_back(ConstantInt::get(*TheContext, APInt(64, 0)));
        gepAccess.push_back(ConstantInt::get(*TheContext, APInt(64, methodIndex)));


        Value *methodsPtr = Builder->CreateStructGEP(runtimeFunctionType(), functionObject, 6, "get_methods");
        Value *methodPtr = Builder->CreateGEP(ArrayType::get(runtimeFunctionMethodType(), 0), 
                                              methodsPtr, 
                                              ArrayRef(gepAccess),
                                              "get_method",
                                              true);
        
        Value *closedOversPtr1 = Builder->CreateStructGEP(runtimeFunctionMethodType(), methodPtr, 5, "get_closed_overs");
        auto closedOversPtr = Builder->CreateLoad(dynamicBoxedType(), closedOversPtr1, "load_closed_overs");
        
        for(int i=0; i<method.closedovers_size(); i++) {
          auto name = method.closedovers(i).subnode().local().name();
          auto closedOverType = closedOvers[i];
          if(functionArgTypes.find(name) == functionArgTypes.end()) {
            
            auto closedOverPtr = Builder->CreateGEP(ArrayType::get(dynamicBoxedType(), method.closedovers_size()), 
                                                    closedOversPtr, 
                                                    ArrayRef((Value *)ConstantInt::get(*TheContext, APInt(64, i))),
                                                    "get_closed_over",
                                                    true);
            
            auto closedOverVal = Builder->CreateLoad(dynamicBoxedType(), closedOverPtr, "load_closed_over");
            auto closedOver = TypedValue(closedOverType, closedOverVal);            
            if(!closedOver.first.isBoxedScalar()) dynamicRetain(closedOver.second);
            auto unboxed = unbox(closedOver);
            functionArgTypes.insert({name, unboxed.first});      
            namedFunctionArgs.insert({name, unboxed});
          }
        }
      }
      
      VariableBindingTypesStack.push_back(functionArgTypes);
      VariableBindingStack.push_back(namedFunctionArgs);
      
      /* The actual code generation happens here! */
      
      try {
        auto result = codegen(method.body(), retType);
        // We consume the function object being called (by convention).
        dynamicRelease(functionObject, false);
        Builder->CreateRet(retType.isBoxedScalar() ? box(result).second : result.second);
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
      
    } else {
      /* This is a case when return type computed from recursion does not match the one 
         that is expected of the function. In this case, we need to call the function with the discovered type and convert the return type afterwards */
      auto f = make_unique<FunctionJIT>();
      f->methodIndex = methodIndex;
      f->args = args;
      f->retVal = realRetType;
      f->uniqueId = uniqueId;
      f->closedOvers = closedOvers;
      ExitOnErr(TheJIT->addAST(std::move(f)));
      

      vector<TypedValue> functionArgs;
      for(int i=0; i<method.params_size(); i++) {
        auto value = TypedValue(args[i].removeConst(), fArgs[i]);
        functionArgs.push_back(value);
      }
      /* Instead of consuming the function object we pass it on */
      functionArgs.push_back(TypedValue(ObjectTypeSet(functionType), fArgs[method.params_size()]));

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
      verifyFunction(*CalleeF);
    }
  } // !CalleeF
}
