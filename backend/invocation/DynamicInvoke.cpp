#include "../codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "../cljassert.h"

using namespace std;
using namespace llvm;

extern "C" {
  #include "../runtime/Keyword.h"
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
  for(unsigned long i=0; i<args.size(); i++) {
     auto arg = args[i];
     auto group = i / 8;
     Value *type = nullptr;
     Value *packed = nullptr;
     if(arg.first.isDetermined()) {
       type = ConstantInt::get(*TheContext, APInt(64, arg.first.determinedType(), false));
       packed = ConstantInt::get(*TheContext, APInt(64, arg.first.isScalar() ? 0 : 1, false));
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

  vector<ObjectTypeSet> argTypess;
  for(auto &arg: args) argTypess.push_back(arg.first);
  
  string rName = ObjectTypeSet::fullyQualifiedMethodKey(node.form(), argTypess, retValType);

  Value *callablePointer = Builder->CreatePointerCast(functionPointer.second, FT->getPointerTo());
  auto finalRetVal = Builder->CreateCall(FunctionCallee(FT, callablePointer), argVals, string("call_dynamic_") + rName);

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



