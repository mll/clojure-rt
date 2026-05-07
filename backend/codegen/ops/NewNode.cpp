#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantClass.h"
#include "../CodeGen.h"
#include "../invoke/InvokeManager.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "codegen/TypedValue.h"
#include "runtime/Class.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const NewNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Get class name from type of class node
  auto classTypeSet = getType(subnode.class_(), ObjectTypeSet::all());

  auto *constVal = classTypeSet.getConstant();
  ConstantClass *classConst = dynamic_cast<ConstantClass *>(constVal);
  if (!classConst) {
    throwCodeGenerationException("Unable to resolve classname for new", node);
  }
  string className = classConst->value;
  if (className.find("class ") == 0) {
    className = className.substr(6);
  }

  // 2. Resolve class and its description
  ::Class *cls =
      this->compilerState.classRegistry.getCurrent(className.c_str());
  PtrWrapper<::Class> clsGuard(cls);
  if (!cls) {
    throwCodeGenerationException("Class " + className + " not found", node);
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    throwCodeGenerationException(
        "Class " + className + " does not have compiler metadata", node);
  }

  // 3. Find matching constructors by arity
  std::vector<const IntrinsicDescription *> versions;
  for (const auto &ctor : ext->constructors) {
    if ((int32_t)ctor.argTypes.size() == subnode.args_size()) {
      versions.push_back(&ctor);
    }
  }

  if (versions.empty()) {
    throwCodeGenerationException("Class " + className +
                                     " does not have a constructor of arity " +
                                     to_string(subnode.args_size()),
                                 node);
  }

  // 4. Codegen arguments
  std::vector<TypedValue> args;
  bool allDetermined = true;
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    if (!t.type.isDetermined())
      allDetermined = false;
    args.push_back(t);
    guard.push(t);
  }

  const IntrinsicDescription *bestMatch = nullptr;
  if (allDetermined) {
    // Exact match trial
    for (const auto *ctor : versions) {
      bool match = true;
      for (size_t i = 0; i < args.size(); i++) {
        if (args[i].type.restriction(ctor->argTypes[i]).isEmpty()) {
          match = false;
          break;
        }
      }
      if (match) {
        bestMatch = ctor;
        break;
      }
    }
  }

  if (bestMatch) {
    std::vector<TypedValue> unboxedArgs;
    for (size_t i = 0; i < args.size(); i++) {
      if (!bestMatch->argTypes[i].isBoxedType()) {
        unboxedArgs.push_back(this->valueEncoder.unbox(args[i]));
      } else {
        unboxedArgs.push_back(this->valueEncoder.box(args[i]));
      }
    }
    return this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs,
                                                 &guard);
  }

  // No exact compile-time match -> Dynamic Dispatch or specialized trials
  Function *currentFn = this->Builder.GetInsertBlock()->getParent();
  BasicBlock *mergeBB =
      BasicBlock::Create(this->TheContext, "new_dispatch_merge", currentFn);

  struct DispatchCase {
    BasicBlock *block;
    TypedValue value;
  };
  std::vector<DispatchCase> cases;
  std::vector<Value *> argRuntimeTypes(args.size(), nullptr);

  for (const auto *fid : versions) {
    BasicBlock *thenBB =
        BasicBlock::Create(this->TheContext, "new_specialised", currentFn);
    BasicBlock *nextBB =
        BasicBlock::Create(this->TheContext, "new_specialised_next", currentFn);

    Value *match = nullptr;
    for (size_t i = 0; i < args.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        if (!argRuntimeTypes[i]) {
          TypedValue boxed = this->valueEncoder.box(args[i]);
          FunctionType *getTypeSig = FunctionType::get(
              this->types.wordTy, {this->types.RT_valueTy}, false);
          argRuntimeTypes[i] = this->invokeManager.invokeRaw(
              "getType", getTypeSig, {boxed.value}, &guard, true);
        }

        uint32_t targetTypeID = fid->argTypes[i].determinedType();
        Value *targetVal =
            ConstantInt::get(this->types.i32Ty, (uint32_t)targetTypeID);
        Value *isType = this->Builder.CreateICmpEQ(
            this->Builder.CreateTrunc(argRuntimeTypes[i], this->types.i32Ty),
            targetVal);

        if (match)
          match = this->Builder.CreateAnd(match, isType);
        else
          match = isType;
      }
    }

    if (!match)
      match = this->Builder.getTrue();

    this->Builder.CreateCondBr(match, thenBB, nextBB);
    this->Builder.SetInsertPoint(thenBB);

    std::vector<TypedValue> specializedArgs;
    for (size_t i = 0; i < args.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        TypedValue valToUnbox = args[i];
        if (valToUnbox.type.isBoxedType()) {
          valToUnbox = TypedValue(fid->argTypes[i].boxed(), valToUnbox.value);
        }
        llvm::Value *unboxedVal = this->valueEncoder.unbox(valToUnbox).value;
        specializedArgs.push_back(TypedValue(fid->argTypes[i], unboxedVal));
      } else {
        specializedArgs.push_back(this->valueEncoder.box(args[i]));
      }
    }

    TypedValue res =
        this->invokeManager.generateIntrinsic(*fid, specializedArgs, &guard);
    TypedValue boxedRes = this->valueEncoder.box(res);
    cases.push_back({this->Builder.GetInsertBlock(), boxedRes});
    this->Builder.CreateBr(mergeBB);
    this->Builder.SetInsertPoint(nextBB);
  }

  // Fallback -> No matching constructor found at runtime
  Value *clsName = this->Builder.CreateGlobalString(className, "new_fail_class");
  FunctionType *fnTy = FunctionType::get(
      this->types.voidTy, {this->types.ptrTy, this->types.i32Ty}, false);
  this->invokeManager.invokeRaw(
      "throwNoMatchingConstructorException_C", fnTy,
      {clsName, ConstantInt::get(this->types.i32Ty, subnode.args_size())});
  this->Builder.CreateUnreachable();

  this->Builder.SetInsertPoint(mergeBB);
  PHINode *phi = Builder.CreatePHI(types.RT_valueTy, cases.size(), "new_phi");
  for (auto &c : cases) {
    phi->addIncoming(c.value.value, c.block);
  }

  return TypedValue(ObjectTypeSet((objectType)cls->registerId), phi);
}


ObjectTypeSet CodeGen::getType(const Node &node, const NewNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto classTypeSet = getType(subnode.class_(), ObjectTypeSet::all());
  auto *constVal = classTypeSet.getConstant();
  ConstantClass *classConst = dynamic_cast<ConstantClass *>(constVal);
  if (!classConst) {
    return ObjectTypeSet::dynamicType();
  }

  string className = classConst->value;
  if (className.find("class ") == 0) {
    className = className.substr(6);
  }
  ScopedRef<::Class> cls(this->compilerState.classRegistry.getCurrent(className.c_str()));
  if (!cls)
    return ObjectTypeSet::dynamicType();

  return ObjectTypeSet((objectType)cls->registerId);
}

} // namespace rt
