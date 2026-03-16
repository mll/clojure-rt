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

TypedValue CodeGen::codegen(const Node &node, const InstanceCallNode &subnode,
                             const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Codegen instance
  auto instanceVal = codegen(subnode.instance(), ObjectTypeSet::all());
  guard.push(instanceVal);

  // 2. Codegen arguments
  std::vector<TypedValue> args;
  std::vector<ObjectTypeSet> argTypes;
  bool allDetermined = instanceVal.type.isDetermined();
  
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    if (!t.type.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.type.unboxed());
    args.push_back(t);
    guard.push(t);
  }

  // 3. Resolve Class
  if (!instanceVal.type.isDetermined()) {
    throwCodeGenerationException("Dynamic instance call not implemented yet", node);
  }

  auto objType = instanceVal.type.determinedType();
  
  ::Class *targetClass = this->compilerState.classRegistry.getCurrent((int32_t)objType);

  if (objType == classType && instanceVal.type.getConstant()) {
    if (auto *cc = dynamic_cast<ConstantClass *>(instanceVal.type.getConstant())) {
      ::Class *constantClassPtr = (::Class *)cc->value;
      if (targetClass && targetClass != constantClassPtr) {
        // The constant class is more specific than the generic Class hull
        Ptr_release(targetClass);
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      } else if (!targetClass) {
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      }
    }
  }

  if (!targetClass) {
    string name = ObjectTypeSet::toHumanReadableName(objType);
    targetClass = this->compilerState.classRegistry.getCurrent(name.c_str());
  }

  PtrWrapper<Class> cls(targetClass);
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found for instance type: " << ObjectTypeSet::toHumanReadableName(objType);
    throwCodeGenerationException(oss.str(), node);
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have compiler metadata";
    throwCodeGenerationException(oss.str(), node);
  }

  auto m = subnode.method();
  auto it_method = ext->instanceFns.find(m);
  if (it_method == ext->instanceFns.end()) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have an instance method " << m;
    throwCodeGenerationException(oss.str(), node);
  }

  // 4. Resolve Overloads and Handle Dynamic Dispatch
  const std::vector<IntrinsicDescription> &versions = it_method->second;
  const IntrinsicDescription *bestMatch = nullptr;
  const IntrinsicDescription *generic = nullptr;
  std::vector<const IntrinsicDescription *> specialized;

  // allArgs will be used for dispatch logic to match StaticCallNode pattern
  std::vector<TypedValue> allArgs = {instanceVal};
  allArgs.insert(allArgs.end(), args.begin(), args.end());

  for (const auto &id : versions) {
    if (id.argTypes.size() == allArgs.size()) {
      bool staticallyPossible = true;
      for (size_t i = 0; i < allArgs.size(); i++) {
        if (allArgs[i].type.restriction(id.argTypes[i]).isEmpty()) {
          staticallyPossible = false;
          break;
        }
      }
      if (!staticallyPossible)
        continue;

      // Special case: exact match
      if (allDetermined) {
        bool match = true;
        for (size_t i = 0; i < allArgs.size(); i++) {
          if (allArgs[i].type.restriction(id.argTypes[i]).isEmpty()) {
            match = false;
            break;
          }
        }
        if (match) {
          bestMatch = &id;
          break;
        }
      }

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

  if (bestMatch) {
    std::vector<TypedValue> unboxedArgs;
    for (size_t i = 0; i < allArgs.size(); i++) {
      unboxedArgs.push_back(this->valueEncoder.unbox(allArgs[i]));
    }
    return this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs, &guard);
  }

  if (specialized.empty() && !generic) {
    std::ostringstream ss;
    ss << "No statically possible overloads found for " << ObjectTypeSet::toHumanReadableName(objType) << "." << m;
    throwCodeGenerationException(ss.str(), node);
  }

  // If we have a generic fallback, limit specialized checks to save code size
  if (generic && specialized.size() > 2) {
    specialized.resize(2);
  }

  Function *currentFn = this->Builder.GetInsertBlock()->getParent();
  BasicBlock *mergeBB =
      BasicBlock::Create(this->TheContext, "instance_call_merge", currentFn);

  struct DispatchCase {
    BasicBlock *block;
    TypedValue value;
  };
  std::vector<DispatchCase> cases;

  // Placeholder for lazy runtime types
  std::vector<Value *> argRuntimeTypes(allArgs.size(), nullptr);

  for (const auto *fid : specialized) {
    BasicBlock *thenBB = BasicBlock::Create(
        this->TheContext, "instance_call_specialised", currentFn);
    BasicBlock *nextBB = BasicBlock::Create(
        this->TheContext, "instance_call_specialised_next", currentFn);

    Value *match = nullptr;
    for (size_t i = 0; i < allArgs.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        if (!argRuntimeTypes[i]) {
          TypedValue boxed = this->valueEncoder.box(allArgs[i]);
          ObjectTypeSet retType(integerType, false);
          std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType()};
          std::vector<TypedValue> pArgVals = {boxed};
          TypedValue typeVal = this->invokeManager.invokeRuntime(
              "getType", &retType, pArgTypes, pArgVals);
          argRuntimeTypes[i] = typeVal.value;
        }

        objectType target = fid->argTypes[i].determinedType();
        Value *targetVal = ConstantInt::get(this->types.i32Ty, (uint32_t)target);
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
    for (size_t i = 0; i < allArgs.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        objectType target = fid->argTypes[i].determinedType();
        llvm::Value *unboxedVal = nullptr;
        if (target == integerType)
          unboxedVal = this->valueEncoder.unboxInt32(allArgs[i]).value;
        else if (target == doubleType)
          unboxedVal = this->valueEncoder.unboxDouble(allArgs[i]).value;
        else if (target == booleanType)
          unboxedVal = this->valueEncoder.unboxBool(allArgs[i]).value;
        else if (target == stringType || target == persistentListType ||
                 target == persistentVectorType || target == bigIntegerType ||
                 target == ratioType || target == keywordType ||
                 target == symbolType || target == functionType ||
                 target == classType || target == persistentArrayMapType ||
                 target == varType) {
          unboxedVal = this->valueEncoder.unboxPointer(allArgs[i]).value;
        } else {
          unboxedVal = this->valueEncoder.unbox(allArgs[i]).value;
        }
        specializedArgs.push_back(TypedValue(fid->argTypes[i], unboxedVal));
      } else {
        specializedArgs.push_back(this->valueEncoder.box(allArgs[i]));
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
    TypedValue genRes =
        this->invokeManager.generateIntrinsic(*generic, allArgs, &guard);
    TypedValue boxedGenRes = this->valueEncoder.box(genRes);
    cases.push_back({this->Builder.GetInsertBlock(), boxedGenRes});
    this->Builder.CreateBr(mergeBB);
  } else {
    // No match and no generic -> Runtime Exception
    string typeName = ObjectTypeSet::toHumanReadableName(objType);
    Value *className = this->Builder.CreateGlobalString(typeName, "instance_call_class");
    Value *methName = this->Builder.CreateGlobalString(m, "instance_call_method");

    std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType(),
                                            ObjectTypeSet::dynamicType()};
    std::vector<TypedValue> pArgVals = {
        TypedValue(ObjectTypeSet::dynamicType(), className),
        TypedValue(ObjectTypeSet::dynamicType(), methName)};

    this->invokeManager.invokeRuntime("throwNoMatchingOverloadException_C",
                                      nullptr, pArgTypes, pArgVals);
    this->Builder.CreateUnreachable();
  }

  this->Builder.SetInsertPoint(mergeBB);
  PHINode *phi =
      Builder.CreatePHI(types.RT_valueTy, cases.size(), "instance_call_phi");
  for (auto &c : cases) {
    phi->addIncoming(c.value.value, c.block);
  }

  return TypedValue(ObjectTypeSet::dynamicType(), phi);
}

ObjectTypeSet CodeGen::getType(const Node &node,
                               const InstanceCallNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto instanceType = getType(subnode.instance(), ObjectTypeSet::all());
  std::vector<ObjectTypeSet> argTypes;
  bool allDetermined = instanceType.isDetermined();
  
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    if (!t.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.unboxed());
  }

  if (!instanceType.isDetermined())
    return ObjectTypeSet::dynamicType();

  auto objType = instanceType.determinedType();
  ::Class *targetClass = compilerState.classRegistry.getCurrent((int32_t)objType);

  if (objType == classType && instanceType.getConstant()) {
    if (auto *cc = dynamic_cast<ConstantClass *>(instanceType.getConstant())) {
      ::Class *constantClassPtr = (::Class *)cc->value;
      if (targetClass && targetClass != constantClassPtr) {
        Ptr_release(targetClass);
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      } else if (!targetClass) {
        targetClass = constantClassPtr;
        Ptr_retain(targetClass);
      }
    }
  }

  if (!targetClass) {
    string name = ObjectTypeSet::toHumanReadableName(objType);
    targetClass = compilerState.classRegistry.getCurrent(name.c_str());
  }

  PtrWrapper<Class> cls(targetClass);
  if (!cls)
    return ObjectTypeSet::dynamicType();

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext)
    return ObjectTypeSet::dynamicType();

  auto it_method = ext->instanceFns.find(subnode.method());
  if (it_method == ext->instanceFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  if (allDetermined) {
    for (const auto &id : versions) {
      if (id.argTypes.size() == argTypes.size() + 1) {
        bool match = true;
        if (instanceType.restriction(id.argTypes[0]).isEmpty()) {
          match = false;
        } else {
          for (size_t i = 0; i < argTypes.size(); i++) {
            if (argTypes[i].restriction(id.argTypes[i + 1]).isEmpty()) {
              match = false;
              break;
            }
          }
        }
        if (match) {
          // Fold instance call if possible
          std::vector<ObjectTypeSet> allArgs = {instanceType};
          allArgs.insert(allArgs.end(), argTypes.begin(), argTypes.end());
          ObjectTypeSet folded = invokeManager.foldIntrinsic(id, allArgs);
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