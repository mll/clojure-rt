#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../../tools/RandomID.h"
#include "../../types/ConstantClass.h"
#include "../CodeGen.h"
#include "InvokeManager.h"
#include "llvm/IR/MDBuilder.h"
#include <cstdint>
#include <sstream>

using namespace llvm;
using namespace std;

namespace rt {

TypedValue InvokeManager::generateInstanceCall(
    const std::string &methodName, TypedValue instance,
    const std::vector<TypedValue> &args, CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  if (!instance.type.isDetermined()) {
    return generateDynamicInstanceCall(methodName, instance, args, guard, node);
  } else {
    return generateDeterminedInstanceCall(methodName, instance, args, guard,
                                          node);
  }
}

TypedValue InvokeManager::generateDynamicInstanceCall(
    const std::string &methodName, TypedValue instance,
    const std::vector<TypedValue> &args, CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  auto &TheContext = theModule.getContext();
  Function *currentFn = this->builder.GetInsertBlock()->getParent();
  if (args.size() >= 64) {
    throwInternalInconsistencyException(
        "Instance call with more than 63 arguments is not supported in dynamic "
        "mode");
  }

  // cout << "Generating dynamic instance call for method: " << methodName <<
  // endl; Allocate an Inline Cache slot as a global variable in the module
  auto *slotTy = StructType::get(TheContext, {types.wordTy, types.ptrTy});

  std::stringstream ss;
  ss << "__ic_slot_cg_" << &this->codeGen << "_" << generateRandomHex(16);
  if (node) {
    ss << "_node_" << node;
  }
  std::string slotName = ss.str();

  auto *slotGlobal =
      new GlobalVariable(theModule, slotTy, false, GlobalValue::InternalLinkage,
                         ConstantAggregateZero::get(slotTy), slotName);
  unsigned int wordSizeBits = types.wordTy->getPrimitiveSizeInBits();
  slotGlobal->setAlignment(MaybeAlign(wordSizeBits / 4));
  Value *slotPtr = slotGlobal;

  // Define the bridge signature based on call-site knowledge
  std::vector<Type *> llvmArgTypes;
  llvmArgTypes.push_back(types.RT_valueTy); // Instance
  uint64_t boxedMask = 0;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i].type.isDetermined()) {
      uint32_t t = args[i].type.determinedType();
      if (t == integerType)
        llvmArgTypes.push_back(types.i32Ty);
      else if (t == doubleType)
        llvmArgTypes.push_back(types.doubleTy);
      else if (t == booleanType)
        llvmArgTypes.push_back(types.i1Ty);
      else if (t == keywordType || t == symbolType || t == nilType)
        llvmArgTypes.push_back(types.RT_valueTy);
      else
        llvmArgTypes.push_back(types.ptrTy);
    } else {
      llvmArgTypes.push_back(types.RT_valueTy);
      boxedMask |= (1ULL << i);
    }
  }
  FunctionType *bridgeSig =
      FunctionType::get(types.RT_valueTy, llvmArgTypes, false);

  // 1. Get current instance type
  TypedValue boxedInstance = valueEncoder.box(instance);
  FunctionType *getTypeSig =
      FunctionType::get(types.wordTy, {types.RT_valueTy}, false);
  FunctionCallee getTypeFunc =
      theModule.getOrInsertFunction("getType", getTypeSig);
  Value *currentTypeIdent =
      builder.CreateCall(getTypeFunc, {boxedInstance.value});

  // 2. Check IC hit (Atomic load for consistency)
  unsigned int pairSizeBits = wordSizeBits * 2;
  Type *pairTy = IntegerType::get(TheContext, pairSizeBits);
  LoadInst *pair = builder.CreateAlignedLoad(
      pairTy, slotPtr, MaybeAlign(wordSizeBits / 4), "ic_pair");
  pair->setAtomic(AtomicOrdering::Acquire);

  Value *cachedTag = builder.CreateTrunc(pair, types.wordTy, "cached_tag");
  Value *cachedBridgeVal = builder.CreateLShr(pair, wordSizeBits);
  Value *cachedBridgeIword =
      builder.CreateTrunc(cachedBridgeVal, types.wordTy, "cached_bridge_iword");
  Value *cachedBridge =
      builder.CreateIntToPtr(cachedBridgeIword, types.ptrTy, "cached_bridge");

  BasicBlock *fastPath = BasicBlock::Create(TheContext, "ic_hit", currentFn);
  BasicBlock *slowPath = BasicBlock::Create(TheContext, "ic_miss", currentFn);
  BasicBlock *callBB = BasicBlock::Create(TheContext, "ic_call", currentFn);

  Value *isHit = builder.CreateICmpEQ(cachedTag, currentTypeIdent);
  builder.CreateCondBr(isHit, fastPath, slowPath);

  // --- SLOW PATH: Call Bouncer ---
  builder.SetInsertPoint(slowPath);
  FunctionType *bouncerSig =
      FunctionType::get(types.ptrTy,
                        {types.ptrTy, types.ptrTy, types.i32Ty, types.ptrTy,
                         types.i64Ty, types.ptrTy},
                        false);

  auto *argsArrType = ArrayType::get(types.RT_valueTy, args.size() + 1);
  // Create alloca in entry block for proper dominance and SROA optimization
  llvm::BasicBlock &entryBB = currentFn->getEntryBlock();
  llvm::IRBuilder<> entryBuilder(&entryBB, entryBB.begin());
  entryBuilder.SetCurrentDebugLocation(this->builder.getCurrentDebugLocation());
  auto *argsArray =
      entryBuilder.CreateAlloca(argsArrType, nullptr, "bouncer_args");
  builder.CreateStore(boxedInstance.value,
                      builder.CreateStructGEP(argsArrType, argsArray, 0));
  for (size_t i = 0; i < args.size(); i++) {
    TypedValue b = valueEncoder.box(args[i]);
    builder.CreateStore(b.value,
                        builder.CreateStructGEP(argsArrType, argsArray, i + 1));
  }

  Value *methNameG = builder.CreateGlobalString(methodName);
  Value *jitEnginePtr =
      ConstantInt::get(this->types.i64Ty, (uintptr_t)codeGen.jitEnginePtr);
  jitEnginePtr = builder.CreateIntToPtr(jitEnginePtr, types.ptrTy);

  Value *newBridge = invokeRaw(
      "InstanceCallSlowPath", bouncerSig,
      {slotPtr, methNameG, ConstantInt::get(types.i32Ty, (int32_t)args.size()),
       builder.CreateStructGEP(argsArrType, argsArray, 0),
       ConstantInt::get(types.i64Ty, boxedMask), jitEnginePtr},
      guard, false);
  BasicBlock *slowPathEnd = builder.GetInsertBlock();
  builder.CreateBr(callBB);

  // --- FAST PATH ---
  builder.SetInsertPoint(fastPath);
  builder.CreateBr(callBB);

  // --- EXECUTE BRIDGE ---
  builder.SetInsertPoint(callBB);
  PHINode *bridgePtr = builder.CreatePHI(types.ptrTy, 2, "bridge_to_call");
  bridgePtr->addIncoming(cachedBridge, fastPath);
  bridgePtr->addIncoming(newBridge, slowPathEnd);

  std::vector<Value *> bridgeArgs;
  bridgeArgs.push_back(boxedInstance.value);
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i].type.isDetermined()) {
      uint32_t t = args[i].type.determinedType();
      if (t == integerType)
        bridgeArgs.push_back(valueEncoder.unboxInt32(args[i]).value);
      else if (t == doubleType)
        bridgeArgs.push_back(valueEncoder.unboxDouble(args[i]).value);
      else if (t == booleanType)
        bridgeArgs.push_back(valueEncoder.unboxBool(args[i]).value);
      else if (t == keywordType || t == symbolType || t == nilType)
        bridgeArgs.push_back(valueEncoder.box(args[i]).value);
      else
        bridgeArgs.push_back(valueEncoder.unboxPointer(args[i]).value);
    } else {
      bridgeArgs.push_back(valueEncoder.box(args[i]).value);
    }
  }

  Value *result = invokeRaw(bridgePtr, bridgeSig, bridgeArgs, guard);
  return TypedValue(ObjectTypeSet::dynamicType(), result);
}

TypedValue InvokeManager::generateDeterminedInstanceCall(
    const std::string &methodName, TypedValue instance,
    const std::vector<TypedValue> &args, CleanupChainGuard *guard,
    const clojure::rt::protobuf::bytecode::Node *node) {
  auto objType = instance.type.determinedType();

  ::Class *targetClass =
      this->compilerState.classRegistry.getCurrent((int32_t)objType);

  PtrWrapper<Class> cls(targetClass);
  if (!cls) {
    std::ostringstream oss;
    oss << "Class not found for instance type: "
        << ObjectTypeSet::toHumanReadableName(objType);
    if (node)
      throwCodeGenerationException(oss.str(), *node);
    else
      throwInternalInconsistencyException(oss.str());
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType)
        << " does not have compiler metadata";
    if (node)
      throwCodeGenerationException(oss.str(), *node);
    else
      throwInternalInconsistencyException(oss.str());
  }

  // 1. Resolve Overloads and Handle Dynamic Dispatch
  const std::vector<IntrinsicDescription> *versions_ptr = nullptr;
  ClassDescription *curr_ext = ext;
  ::Class *curr_cls = cls.get();
  Ptr_retain(curr_cls);

  while (curr_ext) {
    auto it = curr_ext->instanceFns.find(methodName);
    if (it != curr_ext->instanceFns.end()) {
      versions_ptr = &it->second;
      break;
    }

    if (curr_ext->parentName.empty() ||
        curr_ext->parentName == curr_ext->name) {
      break;
    }

    ::Class *parentCls = this->compilerState.classRegistry.getCurrent(
        curr_ext->parentName.c_str());
    Ptr_release(curr_cls);
    curr_cls = parentCls;
    if (!curr_cls) {
      curr_ext = nullptr;
      break;
    }
    curr_ext = static_cast<ClassDescription *>(curr_cls->compilerExtension);
  }

  if (!versions_ptr) {
    if (curr_cls)
      Ptr_release(curr_cls);
    std::ostringstream oss;
    oss << "Class " << ObjectTypeSet::toHumanReadableName(objType)
        << " (or any of its parents) does not have an instance method "
        << methodName;
    if (node)
      throwCodeGenerationException(oss.str(), *node);
    else
      throwInternalInconsistencyException(oss.str());
  }

  const std::vector<IntrinsicDescription> &versions = *versions_ptr;
  const IntrinsicDescription *bestMatch = nullptr;
  const IntrinsicDescription *generic = nullptr;
  std::vector<const IntrinsicDescription *> specialized;

  // We must release curr_cls before return/throw but we need to keep
  // versions_ptr valid until we are done with it. Since versions_ptr points
  // INTO ClassDescription which is owned by the Class in the registry, we
  // should keep curr_cls alive until the end of this function or until we've
  // used versions. Using a PtrWrapper for safety.
  PtrWrapper<::Class> cls_guard(curr_cls);

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
    ss << "No statically possible overloads found for "
       << ObjectTypeSet::toHumanReadableName(objType) << "." << methodName;
    if (node)
      throwCodeGenerationException(ss.str(), *node);
    else
      throwInternalInconsistencyException(ss.str());
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
    BasicBlock *thenBB =
        BasicBlock::Create(TheContext, "instance_call_specialised", currentFn);
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
          TypedValue typeVal =
              this->invokeRuntime("getType", &retType, pArgTypes, pArgVals);
          argRuntimeTypes[i] = typeVal.value;
        }

        uint32_t target = fid->argTypes[i].determinedType();
        Value *targetVal =
            ConstantInt::get(this->types.i32Ty, (uint32_t)target);
        Value *isType = this->builder.CreateICmpEQ(
            this->builder.CreateTrunc(argRuntimeTypes[i], this->types.i32Ty),
            targetVal);

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
        uint32_t target = fid->argTypes[i].determinedType();
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

    TypedValue res = this->generateIntrinsic(*fid, specializedArgs, guard);

    TypedValue boxedRes = this->valueEncoder.box(res);
    cases.push_back({this->builder.GetInsertBlock(), boxedRes});
    this->builder.CreateBr(mergeBB);
    this->builder.SetInsertPoint(nextBB);
  }

  if (generic) {
    TypedValue genRes = this->generateIntrinsic(*generic, allArgs, guard);
    TypedValue boxedGenRes = this->valueEncoder.box(genRes);
    cases.push_back({this->builder.GetInsertBlock(), boxedGenRes});
    this->builder.CreateBr(mergeBB);
  } else {
    // No match and no generic -> Runtime Exception
    string typeName = ObjectTypeSet::toHumanReadableName(objType);
    Value *classNameVal =
        this->builder.CreateGlobalString(typeName, "instance_call_class");
    Value *methNameVal =
        this->builder.CreateGlobalString(methodName, "instance_call_method");

    FunctionType *fnTy = FunctionType::get(
        this->types.voidTy, {this->types.ptrTy, this->types.ptrTy}, false);
    this->invokeRaw("throwNoMatchingOverloadException_C", fnTy,
                    {classNameVal, methNameVal}, guard, false);
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

ObjectTypeSet
InvokeManager::predictInstanceCallType(const std::string &methodName,
                                       const ObjectTypeSet &instanceType,
                                       const std::vector<ObjectTypeSet> &args) {
  if (!instanceType.isDetermined())
    return ObjectTypeSet::dynamicType();

  auto objType = instanceType.determinedType();
  ::Class *targetClass =
      compilerState.classRegistry.getCurrent((int32_t)objType);

  PtrWrapper<Class> cls(targetClass);
  if (!cls)
    return ObjectTypeSet::dynamicType();

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext)
    return ObjectTypeSet::dynamicType();

  auto it_method = ext->instanceFns.find(methodName);
  if (it_method == ext->instanceFns.end())
    return ObjectTypeSet::dynamicType();

  const std::vector<IntrinsicDescription> *versions_ptr = nullptr;
  ClassDescription *curr_ext = ext;
  ::Class *curr_cls = cls.get();
  Ptr_retain(curr_cls);

  while (curr_ext) {
    auto it = curr_ext->instanceFns.find(methodName);
    if (it != curr_ext->instanceFns.end()) {
      versions_ptr = &it->second;
      break;
    }

    if (curr_ext->parentName.empty() ||
        curr_ext->parentName == curr_ext->name) {
      break;
    }

    ::Class *parentCls =
        compilerState.classRegistry.getCurrent(curr_ext->parentName.c_str());
    Ptr_release(curr_cls);
    curr_cls = parentCls;
    if (!curr_cls) {
      curr_ext = nullptr;
      break;
    }
    curr_ext = static_cast<ClassDescription *>(curr_cls->compilerExtension);
  }

  bool allDetermined = instanceType.isDetermined();
  for (const auto &a : args) {
    if (!a.isDetermined()) {
      allDetermined = false;
      break;
    }
  }

  if (versions_ptr && allDetermined) {
    PtrWrapper<::Class> cls_guard(curr_cls);
    const std::vector<IntrinsicDescription> &versions = *versions_ptr;
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
