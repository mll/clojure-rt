#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const IfNode &subnode) {
  auto test = codegen(subnode.test());
  if (test.first == nilType) {
    /* Static analysis - nil is always false, we immediately go else or nil, and since nil is uniqe,
       we can reuse test */ 
    if(subnode.has_else_()) return codegen(subnode.else_());
    return test;
  }
  
  Value *condValue = nullptr;

  if(test.first == booleanType) condValue = test.second;
  if(test.first == runtimeDeterminedType) condValue = dynamicCond(test.second);

  if(condValue) {
    /* Standard if condition */
    Function *parentFunction = Builder->GetInsertBlock()->getParent();
    
    //  create basic blocks
    BasicBlock *thenBB = llvm::BasicBlock::Create(*TheContext, "then", parentFunction);
    BasicBlock *elseBB = llvm::BasicBlock::Create(*TheContext, "else");
    BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");
    
    Builder->CreateCondBr(condValue, thenBB, elseBB);
    
    // then basic block
    Builder->SetInsertPoint(thenBB);
    // then val is that of last value in block
    auto thenWithType = codegen(subnode.then());
 
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

    if(subnode.has_else_()) elseWithType = codegen(subnode.else_());
    else elseWithType = TypedValue(nilType, dynamicNil());
        
    // ditto reasoning to then block
    elseBB = Builder->GetInsertBlock();

    objectType returnType;

    if (elseWithType.first == thenWithType.first) {
      /* we just close off both blocks */
      returnType = elseWithType.first;
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);
      Builder->CreateBr(mergeBB);  
    } else {
      /* We box any fast types (boxing leaves complex types unaffected */ 
      returnType = runtimeDeterminedType;
      elseWithType.second = box(elseWithType);
      Builder->CreateBr(mergeBB);  
      Builder->SetInsertPoint(thenBB);
      thenWithType.second = box(thenWithType);
      Builder->CreateBr(mergeBB); 
      /* For consistency, not really needed: */
      thenWithType.first = runtimeDeterminedType;
      elseWithType.first = runtimeDeterminedType;
    }

    // merge block
    parentFunction->getBasicBlockList().push_back(mergeBB);
    Builder->SetInsertPoint(mergeBB);
          
    llvm::PHINode *phiNode = Builder->CreatePHI(thenWithType.second->getType(), 2, "iftmp");
    phiNode->addIncoming(thenWithType.second, thenBB);
    phiNode->addIncoming(elseWithType.second, elseBB);
    return TypedValue(returnType, phiNode);
  } 

  /* Anything else - immediately true */
  return codegen(subnode.then());
}

