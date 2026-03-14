#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "codegen/TypedValue.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const StaticCallNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  std::vector<ObjectTypeSet> argTypes;
  std::vector<TypedValue> args;
  bool allDetermined = true;
  CleanupChainGuard guard(*this);
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    if (!t.type.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.type.unboxed());
    args.push_back(t);
    guard.push(t);
  }



  auto c = subnode.class_();
  auto m = subnode.method();
  string name = (c.rfind("class ", 0) == 0) ? c.substr(6) : c;

  PtrWrapper<Class> cls(
      this->compilerState.classRegistry.getCurrent(name.c_str()));
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found: " << name;
    throwCodeGenerationException(oss.str(), node);
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << name << " does not have compiler metadata";
    throwCodeGenerationException(oss.str(), node);
  }

  auto it_method = ext->staticFns.find(m);
  if (it_method == ext->staticFns.end()) {
    std::ostringstream oss;
    oss << "Class " << name << " does not have a static method " << m;
    throwCodeGenerationException(oss.str(), node);
  }

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  const IntrinsicDescription *bestMatch = nullptr;

  if (allDetermined) {
    // 1. Try exact match
    for (const auto &id : versions) {
      if (id.argTypes.size() == args.size()) {
        bool match = true;
        for (size_t i = 0; i < args.size(); i++) {
          if (argTypes[i].restriction(id.argTypes[i]).isEmpty()) {
            match = false;
            break;
          }
        }
        if (match) {
          bestMatch = &id;
          break;
        }
      }
    }
  }

  if (bestMatch) {
    std::vector<TypedValue> unboxedArgs;
    for (size_t i = 0; i < args.size(); i++) {
      unboxedArgs.push_back(this->valueEncoder.unbox(args[i]));
    }
    auto result =
        this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs, &guard);


    return result;
  }
  if (allDetermined) {
    std::ostringstream oss;
    oss << "No matching overload found for " << name << "/" << m
        << " with arguments: [";
    for (size_t i = 0; i < argTypes.size(); i++) {
      oss << argTypes[i].toString() << (i == argTypes.size() - 1 ? "" : ", ");
    }
    oss << "]";
    throwCodeGenerationException(oss.str(), node);
  }

  // No compile-time match found or not all determined -> Dynamic Dispatch
  const IntrinsicDescription *generic = nullptr;
  std::vector<const IntrinsicDescription *> specialized;

  for (const auto &id : versions) {
    if (id.argTypes.size() == args.size()) {
      bool staticallyPossible = true;
      for (size_t i = 0; i < args.size(); i++) {
        if (args[i].type.restriction(id.argTypes[i]).isEmpty()) {
          staticallyPossible = false;
          break;
        }
      }
      if (!staticallyPossible)
        continue;

      bool isGeneric = true;
      for (const auto &at : id.argTypes) {
        if (!at.isDynamic()) {
          isGeneric = false;
          break;
        }
      }
      if (isGeneric) {
        generic = &id;
      } else {
        specialized.push_back(&id);
      }
    }
  }

  if (specialized.empty() && !generic) {
    std::ostringstream ss;
    ss << "No statically possible overloads found for " << name << "/" << m;
    throwCodeGenerationException(ss.str(), node);
  }

  // If we have a generic fallback, limit specialized checks to save code size
  if (generic && specialized.size() > 2) {
    specialized.resize(2);
  }

  Function *currentFn = this->Builder.GetInsertBlock()->getParent();
  BasicBlock *mergeBB =
      BasicBlock::Create(this->TheContext, "static_call_merge", currentFn);

  struct DispatchCase {
    BasicBlock *block;
    TypedValue value;
  };
  std::vector<DispatchCase> cases;

  // Pre-fetch runtime types for all arguments once
  std::vector<Value *> argRuntimeTypes;
  for (size_t i = 0; i < args.size(); i++) {
    TypedValue boxed = this->valueEncoder.box(args[i]);
    ObjectTypeSet retType(integerType, false);
    std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType()};
    std::vector<TypedValue> pArgVals = {boxed};
    TypedValue typeVal = this->invokeManager.invokeRuntime("getType", &retType,
                                                           pArgTypes, pArgVals);
    argRuntimeTypes.push_back(typeVal.value);
  }

  for (const auto *fid : specialized) {
    BasicBlock *thenBB = BasicBlock::Create(
        this->TheContext, "static_call_specialised", currentFn);
    BasicBlock *nextBB = BasicBlock::Create(
        this->TheContext, "static_call_specialised_next", currentFn);

    Value *match = nullptr;
    for (size_t i = 0; i < args.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        objectType target = fid->argTypes[i].determinedType();
        Value *targetVal =
            ConstantInt::get(this->types.i32Ty, (uint32_t)target);
        Value *isType =
            this->Builder.CreateICmpEQ(argRuntimeTypes[i], targetVal);

        if (match) {
          match = this->Builder.CreateAnd(match, isType);
        } else {
          match = isType;
        }
      }
    }

    if (!match) {
      match = this->Builder.getTrue();
    }

    this->Builder.CreateCondBr(match, thenBB, nextBB);
    this->Builder.SetInsertPoint(thenBB);

    std::vector<TypedValue> specializedArgs;
    for (size_t i = 0; i < args.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        objectType target = fid->argTypes[i].determinedType();
        llvm::Value *unboxedVal = nullptr;
        if (target == integerType)
          unboxedVal = this->valueEncoder.unboxInt32(args[i]).value;
        else if (target == doubleType)
          unboxedVal = this->valueEncoder.unboxDouble(args[i]).value;
        else if (target == booleanType)
          unboxedVal = this->valueEncoder.unboxBool(args[i]).value;
        else if (target == stringType || target == persistentListType ||
                 target == persistentVectorType || target == bigIntegerType ||
                 target == ratioType || target == keywordType ||
                 target == symbolType || target == functionType ||
                 target == classType || target == persistentArrayMapType ||
                 target == varType) {
          unboxedVal = this->valueEncoder.unboxPointer(args[i]).value;
        } else {
          unboxedVal = this->valueEncoder.unbox(args[i]).value;
        }
        // CRITICAL: Set the verified type so InvokeManager doesn't complain
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

  if (generic) {
    TypedValue genRes = this->invokeManager.generateIntrinsic(*generic, args, &guard);
    TypedValue boxedGenRes = this->valueEncoder.box(genRes);
    cases.push_back({this->Builder.GetInsertBlock(), boxedGenRes});
    this->Builder.CreateBr(mergeBB);
  } else {
    // No match and no generic -> Runtime Exception
    Value *clsName =
        this->Builder.CreateGlobalString(name, "static_call_class");
    Value *methName = this->Builder.CreateGlobalString(m, "static_call_method");

    std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType(),
                                            ObjectTypeSet::dynamicType()};
    std::vector<TypedValue> pArgVals = {
        TypedValue(ObjectTypeSet::dynamicType(), clsName),
        TypedValue(ObjectTypeSet::dynamicType(), methName)};

    // No manual release here - landing pad will handle it via shadow stack

    this->invokeManager.invokeRuntime("throwNoMatchingOverloadException_C",
                                      nullptr, pArgTypes, pArgVals);
    this->Builder.CreateUnreachable();
  }

  this->Builder.SetInsertPoint(mergeBB);
  PHINode *phi =
      Builder.CreatePHI(types.RT_valueTy, cases.size(), "static_call_phi");
  for (auto &c : cases) {
    phi->addIncoming(c.value.value, c.block);
  }



  return TypedValue(ObjectTypeSet::dynamicType(), phi);
}

ObjectTypeSet CodeGen::getType(const Node &node, const StaticCallNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  std::vector<ObjectTypeSet> argTypes;
  bool allDetermined = true;
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    if (!t.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.unboxed());
  }

  auto c = subnode.class_();
  auto m = subnode.method();
  string name = (c.rfind("class ", 0) == 0) ? c.substr(6) : c;

  PtrWrapper<Class> cls(compilerState.classRegistry.getCurrent(name.c_str()));
  if (!cls)
    return ObjectTypeSet::dynamicType();

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext)
    return ObjectTypeSet::dynamicType();

  auto it_method = ext->staticFns.find(m);
  if (it_method == ext->staticFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  if (allDetermined) {
    // 1. Try exact match
    for (const auto &id : versions) {
      if (id.argTypes.size() == argTypes.size()) {
        bool match = true;
        for (size_t i = 0; i < argTypes.size(); i++) {
          if (argTypes[i].restriction(id.argTypes[i]).isEmpty()) {
            match = false;
            break;
          }
        }
        if (match) {
          ObjectTypeSet folded = invokeManager.foldIntrinsic(id, argTypes);
          if (folded.isDetermined() && folded.getConstant()) {
            return folded;
          }
          return id.returnType;
        }
      }
    }
  }

  return ObjectTypeSet::dynamicType();
}

} // namespace rt
