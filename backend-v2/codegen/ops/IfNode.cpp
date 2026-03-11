#include "../CodeGen.h"

using namespace std;
using namespace llvm;

namespace rt {

static Value *toTruthyCondition(LLVMContext &TheContext, IRBuilder<> &Builder,
                                ValueEncoder &valueEncoder, Value *boxedVal) {
  TypedValue boxed(ObjectTypeSet::all().boxed(), boxedVal);
  Value *isNil = valueEncoder.isNil(boxed).value;

  Function *currentFn = Builder.GetInsertBlock()->getParent();
  BasicBlock *notNilBB =
      BasicBlock::Create(TheContext, "cond_not_nil", currentFn);
  BasicBlock *mergeBB = BasicBlock::Create(TheContext, "cond_merge", currentFn);

  Builder.CreateCondBr(isNil, mergeBB, notNilBB);
  BasicBlock *nilBB = Builder.GetInsertBlock();

  Builder.SetInsertPoint(notNilBB);
  Value *isBool = valueEncoder.isBool(boxed).value;
  BasicBlock *checkTrueBB =
      BasicBlock::Create(TheContext, "cond_check_true", currentFn);

  Builder.CreateCondBr(isBool, checkTrueBB, mergeBB);
  BasicBlock *notBoolBB = Builder.GetInsertBlock();

  Builder.SetInsertPoint(checkTrueBB);
  Value *boolVal = valueEncoder.unboxBool(boxed).value;
  Builder.CreateBr(mergeBB);
  BasicBlock *boolBB = Builder.GetInsertBlock();

  Builder.SetInsertPoint(mergeBB);
  PHINode *phi = Builder.CreatePHI(Builder.getInt1Ty(), 3, "truthy_phi");
  phi->addIncoming(Builder.getFalse(), nilBB);
  phi->addIncoming(Builder.getTrue(), notBoolBB);
  phi->addIncoming(boolVal, boolBB);

  return phi;
}

TypedValue CodeGen::codegen(const Node &node, const IfNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto testType = getType(subnode.test(), ObjectTypeSet::all());
  auto thenType = getType(subnode.then(), typeRestrictions);
  auto elseType = subnode.has_else_()
                      ? getType(subnode.else_(), typeRestrictions)
                      : ObjectTypeSet(nilType);

  thenType = thenType.restriction(typeRestrictions);
  elseType = elseType.restriction(typeRestrictions);

  Function *parentFunction = Builder.GetInsertBlock()->getParent();

  if (!testType.contains(nilType) && !testType.contains(booleanType) &&
      !testType.isEmpty()) {
    if (thenType.isEmpty())
      throwCodeGenerationException(
          string("Incorrect type: 'then' branch of if cannot fulfil type "
                 "restrictions: ") +
              typeRestrictions.toString(),
          node);

    auto test = codegen(subnode.test(), ObjectTypeSet::all());
    if (!test.type.isScalar())
      memoryManagement.dynamicRelease(test);

    return codegen(subnode.then(), typeRestrictions);
  }

  if (testType.isDetermined() && testType.determinedType() == nilType) {
    /* Test condition is of single type, which is nil - we immediately know test
     * fails and else branch is triggered! */
    if (elseType.isEmpty())
      throwCodeGenerationException(
          string("Incorrect type: 'else' branch of if cannot fulfil type "
                 "restrictions: ") +
              typeRestrictions.toString(),
          node);

    auto test = codegen(subnode.test(), ObjectTypeSet::all());
    if (!test.type.isScalar())
      memoryManagement.dynamicRelease(test);

    return subnode.has_else_() ? codegen(subnode.else_(), typeRestrictions)
                               : TypedValue(ObjectTypeSet(nilType),
                                            valueEncoder.boxNil().value);
  }

  ConstantBoolean *CI = nullptr;
  if (testType.getConstant() &&
      (CI = dynamic_cast<ConstantBoolean *>(testType.getConstant()))) {
    auto test = codegen(subnode.test(), ObjectTypeSet::all());
    if (!test.type.isScalar())
      memoryManagement.dynamicRelease(test);
    /* In case of a constant (which often arises from constant folding!) we can
     * immediately make a decision on the *compiler* level! */
    bool constCondition = CI->value;
    if (constCondition) {
      if (thenType.isEmpty())
        throwCodeGenerationException(
            string("'then' branch of if expression contains a value that does "
                   "not match allowed types: ") +
                typeRestrictions.toString(),
            node);

      return codegen(subnode.then(), typeRestrictions);
    } else {
      if (elseType.isEmpty())
        throwCodeGenerationException(
            string("'else' branch of if expression contains a value that does "
                   "not match allowed types: ") +
                typeRestrictions.toString(),
            node);

      if (subnode.has_else_())
        return codegen(subnode.else_(), typeRestrictions);
      else
        return TypedValue(ObjectTypeSet(nilType), valueEncoder.boxNil().value);
    }
  }

  /* Standard if condition */

  auto test = codegen(subnode.test(), ObjectTypeSet::all());
  Value *condValue = test.value;

  if (!condValue)
    throwCodeGenerationException(string("Internal error 1"), node);

  if (!test.type.isDetermined())
    condValue =
        toTruthyCondition(TheContext, Builder, valueEncoder, test.value);

  else if (test.type.determinedType() == booleanType)
    condValue = test.value;

  //  create basic blocks
  BasicBlock *thenBB =
      llvm::BasicBlock::Create(TheContext, "then", parentFunction);
  BasicBlock *elseBB =
      llvm::BasicBlock::Create(TheContext, "else", parentFunction);

  Builder.CreateCondBr(condValue, thenBB, elseBB);

  // then basic block
  Builder.SetInsertPoint(thenBB);
  // release test value
  if (!test.type.isScalar())
    memoryManagement.dynamicRelease(test);
  // then val is that of last value in block
  auto thenWithType = thenType.isEmpty()
                          ? TypedValue(thenType, nullptr)
                          : codegen(subnode.then(), typeRestrictions);

  // note that the recursive thenExpr codegen call could change block we're
  // emitting code into.
  thenBB = Builder.GetInsertBlock();
  // Check that block does not return prematurely (in case of recur)
  bool thenShortCircuits =
      !thenWithType.value || thenWithType.value->getType()->isVoidTy();
  /* we leave the then block incomplete so that we can do a cast if types do not
   * match */
  // Builder.CreateBr(mergeBB);

  // else block
  Builder.SetInsertPoint(elseBB);
  // release test value
  if (!test.type.isScalar())
    memoryManagement.dynamicRelease(test);

  // else val is that of last value in block

  TypedValue elseWithType(ObjectTypeSet::empty(), nullptr);

  if (elseType.isEmpty()) {
    elseWithType = TypedValue(elseType, nullptr);
  } else {
    if (subnode.has_else_())
      elseWithType = codegen(subnode.else_(), typeRestrictions);
    else
      elseWithType =
          TypedValue(ObjectTypeSet(nilType), valueEncoder.boxNil().value);
  }

  // ditto reasoning to then block
  elseBB = Builder.GetInsertBlock();
  bool elseShortCircuits =
      !elseWithType.value || elseWithType.value->getType()->isVoidTy();
  bool noShortCircuits = !thenShortCircuits && !elseShortCircuits;

  BasicBlock *mergeBB = nullptr;
  if (noShortCircuits) {
    mergeBB = llvm::BasicBlock::Create(TheContext, "ifcont", parentFunction);
  } else if (thenShortCircuits && !elseShortCircuits) {
    mergeBB = elseBB;
  } else if (!thenShortCircuits && elseShortCircuits) {
    mergeBB = thenBB;
  }

  TypedValue returnTypedValue(ObjectTypeSet::empty(), nullptr);

  if (!elseWithType.type.isEmpty() && !thenWithType.type.isEmpty()) {
    if (elseWithType.type == thenWithType.type) {
      /* we just close off both blocks, same types detected - but const needs to
       * be removed */
      returnTypedValue.type = elseWithType.type.removeConst();
      // If there is one short circuit: in short circuiting block ret, so no br
      // | in the other block no need to jump (only one predecessor)
      if (noShortCircuits) {
        Builder.CreateBr(mergeBB);
        Builder.SetInsertPoint(thenBB);
        Builder.CreateBr(mergeBB);
      }
    } else {
      /* We box any fast types (boxing leaves complex types unaffected) */
      if (!elseShortCircuits) {
        elseWithType = valueEncoder.box(elseWithType);
        if (noShortCircuits)
          Builder.CreateBr(mergeBB);
        returnTypedValue.type =
            returnTypedValue.type.expansion(elseWithType.type);
      }
      Builder.SetInsertPoint(thenBB);
      if (!thenShortCircuits) {
        thenWithType = valueEncoder.box(thenWithType);
        if (noShortCircuits)
          Builder.CreateBr(mergeBB);
        returnTypedValue.type =
            returnTypedValue.type.expansion(thenWithType.type);
      }
    }
  } else {
    if (elseWithType.type.isEmpty() && thenWithType.type.isEmpty())
      throwCodeGenerationException(
          string("Both branches of if cannot fulfil restrictions: ") +
              typeRestrictions.toString(),
          node);

    /* One of the branches does not fit the requirements. We generate an
     * exception if this branch is chosen, but take the other's type in static
     * analysis to speed things up */
    if (elseWithType.type.isEmpty()) {
      returnTypedValue = thenWithType;

      string errMsg =
          string("Runtime exception: 'else' branch of if expression contains a "
                 "value that does not match allowed types: ") +
          typeRestrictions.toString();
      TypedValue msg = dynamicConstructor.createString(errMsg.c_str());
      TypedValue rawPtr = valueEncoder.unboxPointer(msg);
      invokeManager.invokeRuntime("throwInternalInconsistencyException_C",
                                  nullptr, {ObjectTypeSet::all()}, {rawPtr});

      Builder.CreateUnreachable();

      if (noShortCircuits)
        Builder.CreateBr(mergeBB);
      Builder.SetInsertPoint(thenBB);
      if (noShortCircuits)
        Builder.CreateBr(mergeBB);
    }

    if (thenWithType.type.isEmpty()) {
      returnTypedValue = elseWithType;

      if (noShortCircuits)
        Builder.CreateBr(mergeBB);
      Builder.SetInsertPoint(thenBB);

      string errMsg =
          string("Runtime exception: 'then' branch of if expression contains a "
                 "value that does not match allowed types: ") +
          typeRestrictions.toString();
      TypedValue msg = dynamicConstructor.createString(errMsg.c_str());
      TypedValue rawPtr = valueEncoder.unboxPointer(msg);
      invokeManager.invokeRuntime("throwInternalInconsistencyException_C",
                                  nullptr, {ObjectTypeSet::all()}, {rawPtr});

      Builder.CreateUnreachable();

      if (noShortCircuits)
        Builder.CreateBr(mergeBB);
    }
  }

  // merge block
  if (mergeBB)
    Builder.SetInsertPoint(mergeBB);
  if (noShortCircuits && thenWithType.value && elseWithType.value) {
    llvm::PHINode *phiNode =
        Builder.CreatePHI(thenWithType.value->getType(), 2, "iftmp");
    phiNode->addIncoming(thenWithType.value, thenBB);
    phiNode->addIncoming(elseWithType.value, elseBB);
    returnTypedValue.value = phiNode;
    return returnTypedValue;
  }

  if (thenWithType.value)
    return thenWithType;
  return elseWithType;
}

ObjectTypeSet CodeGen::getType(const Node &node, const IfNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto testType = getType(subnode.test(), ObjectTypeSet::all());
  auto thenType = getType(subnode.then(), typeRestrictions);
  auto elseType = subnode.has_else_()
                      ? getType(subnode.else_(), typeRestrictions)
                      : ObjectTypeSet(nilType);
  thenType = thenType.restriction(typeRestrictions);
  elseType = elseType.restriction(typeRestrictions);

  if (!testType.contains(nilType) && !testType.contains(booleanType)) {
    if (thenType.isEmpty())
      throwCodeGenerationException(
          string("Incorrect type: 'then' branch of if cannot fulfil type "
                 "restrictions: ") +
              typeRestrictions.toString(),
          node);
    return thenType;
  }

  ConstantBoolean *CI = nullptr;
  if (testType.isDetermined()) { /* Test condition is of single type */
    switch (testType.determinedType()) {
    case nilType:
      if (elseType.isEmpty())
        throwCodeGenerationException(
            string("Incorrect type: 'else' branch of if cannot fulfil type "
                   "restrictions: ") +
                typeRestrictions.toString(),
            node);
      return elseType;

    case booleanType:
      if (testType.getConstant() &&
          (CI = dynamic_cast<ConstantBoolean *>(testType.getConstant()))) {
        if (CI->value)
          return thenType;
        return elseType;
      }
    default:
      break;
    }
  }

  return thenType.expansion(elseType);
}

} // namespace rt
