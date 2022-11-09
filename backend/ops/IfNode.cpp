#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto testType = getType(subnode.test(), ObjectTypeSet::all());
  auto thenType = getType(subnode.then(), typeRestrictions);
  auto elseType = subnode.has_else_() ? getType(subnode.else_(), typeRestrictions) : ObjectTypeSet(nilType);
  thenType = thenType.restriction(typeRestrictions);
  elseType = elseType.restriction(typeRestrictions);

  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  
  if(!testType.contains(nilType) && !testType.contains(booleanType) && !testType.isEmpty()) {
    if (thenType.isEmpty()) throw CodeGenerationException(string("Incorrect type: 'then' branch of if cannot fulfil type restrictions: ") + typeRestrictions.toString(), node);
    return codegen(subnode.then(), typeRestrictions);
  }

  if(testType.isDetermined() && testType.determinedType() == nilType) { 
    /* Test condition is of single type, which is nil - we immediately know test fails and else branch is triggered! */
    if (elseType.isEmpty()) throw CodeGenerationException(string("Incorrect type: 'else' branch of if cannot fulfil type restrictions: ") + typeRestrictions.toString(), node);
    return subnode.has_else_() ? codegen(subnode.else_(), typeRestrictions) : TypedValue(ObjectTypeSet(nilType, false, new ConstantNil()), dynamicNil());
  }
  
  ConstantBoolean* CI = nullptr;
  if (testType.getConstant() && (CI = dynamic_cast<ConstantBoolean *>(testType.getConstant()))) {
    /* In case of a constant (which often arises from constant folding!) we can immediately make a decision on the *compiler* level! */
    bool constCondition = CI->value;
    if(constCondition) {
      if (thenType.isEmpty()) throw CodeGenerationException(string("'then' branch of if expression contains a value that does not match allowed types: ") + typeRestrictions.toString(), node);
      return codegen(subnode.then(), typeRestrictions);
    } else {
      if (elseType.isEmpty()) throw CodeGenerationException(string("'else' branch of if expression contains a value that does not match allowed types: ") + typeRestrictions.toString(), node);
      if(subnode.has_else_()) return codegen(subnode.else_(), typeRestrictions);
      else return TypedValue(ObjectTypeSet(nilType, false, new ConstantNil()), dynamicNil());
    }
  }

  /* Standard if condition */
  
  auto test = codegen(subnode.test(), ObjectTypeSet::all());
  Value *condValue = nullptr;

  if(!condValue) throw CodeGenerationException(string("Internal error"), node);

  if(!test.first.isDetermined()) condValue = dynamicCond(test.second);
  else if(test.first.determinedType() == booleanType) condValue = test.second;
  

  //  create basic blocks
  BasicBlock *thenBB = llvm::BasicBlock::Create(*TheContext, "then", parentFunction);
  
  BasicBlock *elseBB = llvm::BasicBlock::Create(*TheContext, "else");
  BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");
    
  Builder->CreateCondBr(condValue, thenBB, elseBB);
    
  // then basic block
  Builder->SetInsertPoint(thenBB);
  // then val is that of last value in block
  auto thenWithType = thenType.isEmpty() ? TypedValue(thenType, nullptr) : codegen(subnode.then(), typeRestrictions);
 
  // note that the recursive thenExpr codegen call could change block we're
  // emitting code into.
  thenBB = Builder->GetInsertBlock();
  /* we leave the then block incomplete so that we can do a cast if types do not match */
  //Builder->CreateBr(mergeBB);
    
  // else block
  parentFunction->getBasicBlockList().push_back(elseBB);
  Builder->SetInsertPoint(elseBB);

  // else val is that of last value in block
    
  TypedValue elseWithType;

  if(elseType.isEmpty()) {
    elseWithType = TypedValue(elseType, nullptr);
  } else {
    if(subnode.has_else_()) elseWithType = codegen(subnode.else_(), typeRestrictions);
    else elseWithType = TypedValue(ObjectTypeSet(nilType), dynamicNil());
  }
        
  // ditto reasoning to then block
  elseBB = Builder->GetInsertBlock();

  TypedValue returnTypedValue;
  
  if (!elseWithType.first.isEmpty() && !thenWithType.first.isEmpty()) {
    if (elseWithType.first == thenWithType.first) {
      /* we just close off both blocks, same types detected */
      returnTypedValue.first = elseWithType.first;
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);
      Builder->CreateBr(mergeBB);  
    } else {
      /* We box any fast types (boxing leaves complex types unaffected) */ 
      elseWithType.second = box(elseWithType);
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);
      thenWithType.second = box(thenWithType);
      Builder->CreateBr(mergeBB);
      returnTypedValue.first = elseWithType.first.expansion(thenWithType.first);
    }
  } else {
    if(elseWithType.first.isEmpty() && thenWithType.first.isEmpty()) throw CodeGenerationException(string("Both branches of if cannot fulfil restrictions: ") + typeRestrictions.toString(), node);
    /* One of the branches does not fit the requirements. We generate an exception if this branch is chosen, but take the other's type in static analysis to speed things up */
    if(elseWithType.first.isEmpty()) {
      returnTypedValue = thenWithType;
      
      runtimeException(CodeGenerationException(string("Runtime exception: 'else' branch of if expression contains a value that does not match allowed types: ") + typeRestrictions.toString(), node));
      
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);
      Builder->CreateBr(mergeBB);  
    }

    if(thenWithType.first.isEmpty()) {
      returnTypedValue = elseWithType;
      
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);

      runtimeException(CodeGenerationException(string("Runtime exception: 'then' branch of if expression contains a value that does not match allowed types: ") + typeRestrictions.toString(), node));

      Builder->CreateBr(mergeBB);  
    }
  }

  // merge block
  parentFunction->getBasicBlockList().push_back(mergeBB);
  Builder->SetInsertPoint(mergeBB);
  if(thenWithType.second && elseWithType.second) {        
    llvm::PHINode *phiNode = Builder->CreatePHI(thenWithType.second->getType(), 2, "iftmp");
    phiNode->addIncoming(thenWithType.second, thenBB);
    phiNode->addIncoming(elseWithType.second, elseBB);
    returnTypedValue.second = phiNode;
    return returnTypedValue; 
  }

  if(thenWithType.second) return thenWithType;
  if(!elseWithType.second) throw CodeGenerationException(string("Internal error"), node);
  return elseWithType;
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const IfNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto testType = getType(subnode.test(), ObjectTypeSet::all());
  auto thenType = getType(subnode.then(), typeRestrictions);
  auto elseType = subnode.has_else_() ? getType(subnode.else_(), typeRestrictions) : ObjectTypeSet(nilType);
  thenType = thenType.restriction(typeRestrictions);
  elseType = elseType.restriction(typeRestrictions);
  
  if(!testType.contains(nilType) && !testType.contains(booleanType)) {
    if (thenType.isEmpty()) throw CodeGenerationException(string("Incorrect type: 'then' branch of if cannot fulfil type restrictions: ") + typeRestrictions.toString(), node);
    return thenType;
  }
  ConstantBoolean* CI = nullptr;
  if(testType.isDetermined()) { /* Test condition is of single type */
    switch(testType.determinedType()) {
      case nilType:
        if (elseType.isEmpty()) throw CodeGenerationException(string("Incorrect type: 'else' branch of if cannot fulfil type restrictions: ") + typeRestrictions.toString(), node);
        return elseType;
      case booleanType:
        if(testType.getConstant() && (CI = dynamic_cast<ConstantBoolean *>(testType.getConstant()))) {
          if(CI->value) return thenType;
          return elseType;
        }
      default:
        break;
    }
  }

  return thenType.expansion(elseType);
}
