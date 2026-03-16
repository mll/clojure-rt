#include "InvokeManager.h"
#include "../CodeGen.h"
#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantClass.h"
#include "llvm/IR/MDBuilder.h"
#include <sstream>

using namespace llvm;
using namespace std;

namespace rt {

TypedValue InvokeManager::generateInstanceCall(const std::string &methodName,
                                              TypedValue instance,
                                              const std::vector<TypedValue> &args,
                                              CleanupChainGuard *guard,
                                              const clojure::rt::protobuf::bytecode::Node *node) {
  // 1. Resolve Class
  if (!instance.type.isDetermined()) {
    if (node)
        throwCodeGenerationException("Dynamic instance calls require determined instance type currently", *node);
    else
        throwInternalInconsistencyException("Dynamic instance calls require determined instance type currently");
  }

  auto objType = instance.type.determinedType();
  
  ::Class *targetClass = this->compilerState.classRegistry.getCurrent((int32_t)objType);

  if (objType == classType && instance.type.getConstant()) {
    if (auto *cc = dynamic_cast<ConstantClass *>(instance.type.getConstant())) {
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
    targetClass = this->compilerState.classRegistry.getCurrent(name.c_str());
  }

  PtrWrapper<Class> cls(targetClass);
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found for instance type: " << ObjectTypeSet::toHumanReadableName(objType);
    if (node) throwCodeGenerationException(oss.str(), *node);
    else throwInternalInconsistencyException(oss.str());
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have compiler metadata";
    if (node) throwCodeGenerationException(oss.str(), *node);
    else throwInternalInconsistencyException(oss.str());
  }

  auto it_method = ext->instanceFns.find(methodName);
  if (it_method == ext->instanceFns.end()) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType) << " does not have an instance method " << methodName;
    if (node) throwCodeGenerationException(oss.str(), *node);
    else throwInternalInconsistencyException(oss.str());
  }

  // 2. Resolve Overloads and Handle Dynamic Dispatch
  const std::vector<IntrinsicDescription> &versions = it_method->second;
  const IntrinsicDescription *bestMatch = nullptr;
  const IntrinsicDescription *generic = nullptr;
  std::vector<const IntrinsicDescription *> specialized;

  std::vector<TypedValue> allArgs = {instance};
  allArgs.insert(allArgs.end(), args.begin(), args.end());

  bool allDetermined = true;
  for (const auto &a : allArgs) {
    if (!a.type.isDetermined()) {
        allDetermined = false;
        break;
    }
  }

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
    return this->generateIntrinsic(*bestMatch, unboxedArgs, guard);
  }

  if (specialized.empty() && !generic) {
    std::ostringstream ss;
    ss << "No statically possible overloads found for " << ObjectTypeSet::toHumanReadableName(objType) << "." << methodName;
    if (node) throwCodeGenerationException(ss.str(), *node);
    else throwInternalInconsistencyException(ss.str());
  }

  // If we have a generic fallback, limit specialized checks to save code size
  if (generic && specialized.size() > 2) {
    specialized.resize(2);
  }

  auto &TheContext = theModule.getContext();
  Function *currentFn = this->builder.GetInsertBlock()->getParent();
  BasicBlock *mergeBB =
      BasicBlock::Create(TheContext, "instance_call_merge", currentFn);

  struct DispatchCase {
    BasicBlock *block;
    TypedValue value;
  };
  std::vector<DispatchCase> cases;

  // Placeholder for lazy runtime types
  std::vector<Value *> argRuntimeTypes(allArgs.size(), nullptr);

  for (const auto *fid : specialized) {
    BasicBlock *thenBB = BasicBlock::Create(
        TheContext, "instance_call_specialised", currentFn);
    BasicBlock *nextBB = BasicBlock::Create(
        TheContext, "instance_call_specialised_next", currentFn);

    Value *match = nullptr;
    for (size_t i = 0; i < allArgs.size(); i++) {
      if (fid->argTypes[i].isDetermined()) {
        if (!argRuntimeTypes[i]) {
          TypedValue boxed = this->valueEncoder.box(allArgs[i]);
          ObjectTypeSet retType(integerType, false);
          std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType()};
          std::vector<TypedValue> pArgVals = {boxed};
          TypedValue typeVal = this->invokeRuntime(
              "getType", &retType, pArgTypes, pArgVals);
          argRuntimeTypes[i] = typeVal.value;
        }

        objectType target = fid->argTypes[i].determinedType();
        Value *targetVal = ConstantInt::get(this->types.i32Ty, (uint32_t)target);
        Value *isType =
            this->builder.CreateICmpEQ(argRuntimeTypes[i], targetVal);

        if (match) {
          match = this->builder.CreateAnd(match, isType);
        } else {
          match = isType;
        }
      }
    }

    if (!match) {
      match = this->builder.getTrue();
    }

    this->builder.CreateCondBr(match, thenBB, nextBB);
    this->builder.SetInsertPoint(thenBB);

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
        this->generateIntrinsic(*fid, specializedArgs, guard);

    TypedValue boxedRes = this->valueEncoder.box(res);
    cases.push_back({this->builder.GetInsertBlock(), boxedRes});
    this->builder.CreateBr(mergeBB);
    this->builder.SetInsertPoint(nextBB);
  }

  if (generic) {
    TypedValue genRes =
        this->generateIntrinsic(*generic, allArgs, guard);
    TypedValue boxedGenRes = this->valueEncoder.box(genRes);
    cases.push_back({this->builder.GetInsertBlock(), boxedGenRes});
    this->builder.CreateBr(mergeBB);
  } else {
    // No match and no generic -> Runtime Exception
    string typeName = ObjectTypeSet::toHumanReadableName(objType);
    Value *className = this->builder.CreateGlobalString(typeName, "instance_call_class");
    Value *methName = this->builder.CreateGlobalString(methodName, "instance_call_method");

    std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet::dynamicType(),
                                            ObjectTypeSet::dynamicType()};
    std::vector<TypedValue> pArgVals = {
        TypedValue(ObjectTypeSet::dynamicType(), className),
        TypedValue(ObjectTypeSet::dynamicType(), methName)};

    this->invokeRuntime("throwNoMatchingOverloadException_C",
                                      nullptr, pArgTypes, pArgVals);
    this->builder.CreateUnreachable();
  }

  this->builder.SetInsertPoint(mergeBB);
  PHINode *phi =
      builder.CreatePHI(types.RT_valueTy, cases.size(), "instance_call_phi");
  for (auto &c : cases) {
    phi->addIncoming(c.value.value, c.block);
  }

  return TypedValue(ObjectTypeSet::dynamicType(), phi);
}

ObjectTypeSet InvokeManager::predictInstanceCallType(const std::string &methodName,
                                                   const ObjectTypeSet &instanceType,
                                                   const std::vector<ObjectTypeSet> &args) {
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

  auto it_method = ext->instanceFns.find(methodName);
  if (it_method == ext->instanceFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> &versions = it_method->second;

  bool allDetermined = instanceType.isDetermined();
  for (const auto &a : args) {
    if (!a.isDetermined()) {
      allDetermined = false;
      break;
    }
  }

  if (allDetermined) {
    for (const auto &id : versions) {
      if (id.argTypes.size() == args.size() + 1) {
        bool match = true;
        if (instanceType.restriction(id.argTypes[0]).isEmpty()) {
          match = false;
        } else {
          for (size_t i = 0; i < args.size(); i++) {
            if (args[i].restriction(id.argTypes[i + 1]).isEmpty()) {
              match = false;
              break;
            }
          }
        }
        if (match) {
          // Fold instance call if possible
          std::vector<ObjectTypeSet> allArgs = {instanceType};
          allArgs.insert(allArgs.end(), args.begin(), args.end());
          ObjectTypeSet folded = foldIntrinsic(id, allArgs);
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
