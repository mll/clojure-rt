#include "../codegen.h"

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  auto loopId = subnode.loopid();
  auto op = TheProgramme->RecurType.find(loopId);
  if (op == TheProgramme->RecurType.end()) {
    throw CodeGenerationException("Unexpected recur!", node);
  } else if (op->second == opFn) {
    // TODO: Recur in once - throw exception
    
    auto uniqueId = TheProgramme->RecurTargets.find(loopId)->second;
    auto recurPoint = TheProgramme->Functions.find(uniqueId)->second;
    // Pass fn pointer
    auto callObjectIterator = Builder->GetInsertBlock()->getParent()->arg_end();
    callObjectIterator--;
    Value *callObject = cast<Value>(callObjectIterator);
    
    auto fName = getMangledUniqueFunctionName(uniqueId);
    vector<TypedValue> args;
    for(int i = 0; i < subnode.exprs_size(); i++) {
      auto t = codegen(subnode.exprs(i), ObjectTypeSet::all());
      args.push_back(TypedValue(t.first.removeConst(), t.second));
    }
    bool determinedArgs = true;
    for (auto a: args) if(!a.first.isDetermined()) { determinedArgs = false; break; }
    
    if (determinedArgs) {
      FnNode functionBody = recurPoint.subnode().fn();
      /* We need to find a correct method */

      vector<pair<FnMethodNode, uint64_t>> nodes;
      for(int i=0; i<functionBody.methods_size(); i++) {
        auto &method = functionBody.methods(i).subnode().fnmethod();
        nodes.push_back({method, i});
      }
      
      std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
      {
        if (lhs.first.isvariadic() && !rhs.first.isvariadic()) return false;
        if (!lhs.first.isvariadic() && rhs.first.isvariadic()) return true;
        return lhs.first.fixedarity() > rhs.first.fixedarity();
      });

      int foundIdx = -1;
      for(int i=0; i<nodes.size(); i++) {
        if(nodes[i].first.fixedarity() == args.size()) { foundIdx = i; break;}
        if(nodes[i].first.fixedarity() <= args.size() && nodes[i].first.isvariadic()) { foundIdx = i; break;}
      }
      if (foundIdx == -1) throw CodeGenerationException(string("Function ") + fName + " has been called with wrong arity in recur call: " + to_string(args.size()), node);
      pair<FnMethodNode, uint64_t> method = nodes[foundIdx];
      
      vector<Value *> argVals;
      vector<ObjectTypeSet> argTypes;
      for (auto a: args) {argTypes.push_back(a.first); argVals.push_back(a.second);} // Also handles recur vararg!
      argVals.push_back(callObject);
      
      string rName = ObjectTypeSet::recursiveMethodKey(fName, argTypes);
      string rqName = ObjectTypeSet::fullyQualifiedMethodKey(fName, argTypes, type);
      Function *CalleeF = TheModule->getFunction(rqName);
      if (CalleeF) { // CalleeF might be null in case of recur in variadic
        auto recurCall = Builder->CreateCall(CalleeF, argVals, string("recur_call_") + rName);
        return TypedValue(type, Builder->CreateRet(recurCall));
      }
    }
    
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
    specialisationArgs.push_back(TypedValue(ObjectTypeSet::all(), callObject));
    specialisationArgs.push_back(TypedValue(ObjectTypeSet(integerType), ConstantInt::get(*TheContext, APInt(64, type.isDetermined() ? type.determinedType() : 0, false))));
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
    argVals.push_back(callObject);

    auto retVal = dynamicType(type);
      /* TODO - Return values should be probably handled differently. e.g. if the new function returns something boxed, 
        we should inspect it and decide if we want to use it or not for var invokations */ 

    /* What if the new function returns a different type? we should probably not force the return type. It should be deduced by jit and our job should be to somehow box/unbox it. Alternatively, we can add return type to method name (like fn_1_JJ_LV) and force generation of function that returns the boxed version of needed return type. If it does not match - we should throw a runtime error */ 

    FunctionType *FT = FunctionType::get(retVal, argTypes, false);
    Value *callablePointer = Builder->CreatePointerCast(functionPointer.second, FT->getPointerTo());
    auto finalRetVal = Builder->CreateCall(FunctionCallee(FT, callablePointer), argVals, string("recur_call_dynamic"));
    return TypedValue(type, Builder->CreateRet(finalRetVal));
  } else if (op->second == opLoop) { 
    auto loopRecurPoint = LoopInsertPoints.find(loopId)->second;
    std::vector<TypedValue> values;
    for (int i = 0; i < subnode.exprs_size(); ++i) {
      auto value = codegen(subnode.exprs(i), ObjectTypeSet::all());
      if (value.second->getType() != loopRecurPoint.second[i]->getType()) value = box(value);
      values.push_back(value);
    }
    BasicBlock *currentBB = Builder->GetInsertBlock();
    for (int i = 0; i < subnode.exprs_size(); ++i) {
      loopRecurPoint.second[i]->addIncoming(values[i].second, currentBB);
    }
    Builder->CreateBr(loopRecurPoint.first);
    auto retType = TheProgramme->LoopsBindingsAndRetValInfers.find(loopId)->second.second;
    return TypedValue(retType, nullptr);
  } else if (op->second == opMethod) {
    throw CodeGenerationException("Recur not implemented yet for method!", node);
  }
  
  throw CodeGenerationException("Unexpected recur!", node);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string loopId = subnode.loopid();
  
  auto op = TheProgramme->RecurType.find(loopId);
  if (op == TheProgramme->RecurType.end()) {
    throw CodeGenerationException("Unexpected recur!", node);
  } else if (op->second == opFn) {
    // TODO: Recur in once - throw exception
    auto uniqueId = TheProgramme->RecurTargets.find(loopId)->second;
    auto &recurPoint = TheProgramme->Functions.find(uniqueId)->second;
    InvokeNode *invoke = new InvokeNode();
    Node *bodyCopy = new Node(recurPoint);
    invoke->set_allocated_fn(bodyCopy);
    for(int i=0; i<subnode.exprs_size();i++) {
      Node *arg = invoke->add_args();
      *arg = subnode.exprs(i);
    }

    Subnode *invokeSubnode = new Subnode();
    invokeSubnode->set_allocated_invoke(invoke);

    Node invokeNode = Node(node);
    invokeNode.set_op(opInvoke);
    invokeNode.set_allocated_subnode(invokeSubnode);

    // Memory management??? 
    // This implementation falls into endless loop
     
    return getType(invokeNode, *invoke, typeRestrictions);
  } else if (op->second == opLoop) {
    // Update loop binding types
    auto recurPoint = TheProgramme->LoopsBindingsAndRetValInfers.find(loopId);
    for (int i = 0; i < subnode.exprs_size(); ++i)
      recurPoint->second.first[i] = recurPoint->second.first[i].expansion(getType(subnode.exprs(i), ObjectTypeSet::all()));
    return recurPoint->second.second;
  } else if (op->second == opMethod) {
    throw CodeGenerationException("Recur not implemented yet for method!", node);
  }
  
  throw CodeGenerationException("Unexpected recur!", node);
}
