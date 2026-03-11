#include "../../tools/RTValueWrapper.h"
#include "../CodeGen.h"
#include "bridge/Exceptions.h"
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
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    if (!t.type.isDetermined())
      allDetermined = false;
    argTypes.push_back(t.type.unboxed());
    args.push_back(t);
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
        this->invokeManager.generateIntrinsic(*bestMatch, unboxedArgs);
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

  throwCodeGenerationException(
      "Dynamic dispatch for static calls is not yet supported.", node);
  // No compile-time match found or not all determined -> Dynamic Dispatch
  // const IntrinsicDescription *generic = nullptr;
  // std::vector<const IntrinsicDescription *> specialized;

  // for (const auto &id : versions) {
  //   if (id.argTypes.size() == args.size()) {
  //     bool isGeneric = true;
  //     for (const auto &at : id.argTypes) {
  //       if (!at.isDynamic()) {
  //         isGeneric = false;
  //         break;
  //       }
  //     }
  //     if (isGeneric) {
  //       generic = &id;
  //     } else {
  //       specialized.push_back(&id);
  //     }
  //   }
  // }

  // // If types are determined but NO match found and NO generic fallback ->
  // Fail
  // // early
  // if (allDetermined && !generic) {
  //   std::ostringstream oss;
  //   oss << "No matching overload found for " << name << "/" << m
  //       << " with arguments: [";
  //   for (size_t i = 0; i < argTypes.size(); i++) {
  //     oss << argTypes[i].toString() << (i == argTypes.size() - 1 ? "" : ",
  //     ");
  //   }
  //   oss << "]";
  //   throwCodeGenerationException(oss.str(), node);
  // }

  // // If we have a generic fallback, limit specialized checks to the first two
  // // (fast paths) to save memory
  // if (generic && specialized.size() > 2) {
  //   specialized.resize(2);
  // }

  // // Generate dynamic dispatch cascade
  // Function *currentFn = this->Builder.GetInsertBlock()->getParent();
  // BasicBlock *mergeBB =
  //     BasicBlock::Create(this->TheContext, "dispatch_merge", currentFn);

  // struct DispatchCase {
  //   BasicBlock *bb;
  //   TypedValue result;
  // };
  // std::vector<DispatchCase> cases;

  // for (const auto *fid : specialized) {
  //   BasicBlock *thenBB =
  //       BasicBlock::Create(this->TheContext, "dispatch_then", currentFn);
  //   BasicBlock *nextBB =
  //       BasicBlock::Create(this->TheContext, "dispatch_next", currentFn);

  //   Value *match = this->Builder.getTrue();
  //   for (size_t i = 0; i < args.size(); i++) {
  //     TypedValue boxed = this->valueEncoder.box(args[i]);
  //     Value *isType = nullptr;
  //     objectType target = fid->argTypes[i].determinedType();

  //     if (target == integerType)
  //       isType = this->valueEncoder.isInt32(boxed).value;
  //     else if (target == doubleType)
  //       isType = this->valueEncoder.isDouble(boxed).value;
  //     else if (target == bigIntegerType)
  //       isType = this->valueEncoder.isBigInt(boxed).value;
  //     else if (target == ratioType)
  //       isType = this->valueEncoder.isRatio(boxed).value;

  //     if (isType) {
  //       match = this->Builder.CreateAnd(match, isType);
  //     } else {
  //       match = this->Builder.getFalse();
  //       break;
  //     }
  //   }

  //   this->Builder.CreateCondBr(match, thenBB, nextBB);
  //   this->Builder.SetInsertPoint(thenBB);
  //   TypedValue res = this->invokeManager.generateIntrinsic(*fid, args);
  //   TypedValue boxedRes = this->valueEncoder.box(res);
  //   cases.push_back({this->Builder.GetInsertBlock(), boxedRes});
  //   this->Builder.CreateBr(mergeBB);
  //   this->Builder.SetInsertPoint(nextBB);
  // }

  // // Final fallback
  // if (generic) {
  //   TypedValue genRes = this->invokeManager.generateIntrinsic(*generic,
  //   args); TypedValue boxedGenRes = this->valueEncoder.box(genRes);
  //   cases.push_back({this->Builder.GetInsertBlock(), boxedGenRes});
  //   this->Builder.CreateBr(mergeBB);
  // } else {
  //   // No match and no generic -> Runtime Exception
  //   std::stringstream ss;
  //   ss << "No matching overload found for " << name << "/" << m;
  //   Value *msgVal =
  //       this->Builder.CreateGlobalStringPtr(ss.str(), "dispatch_err_msg");

  //   FunctionType *fnTy =
  //       FunctionType::get(this->types.voidTy, {this->types.ptrTy}, false);
  //   FunctionCallee fn = this->TheModule->getOrInsertFunction(
  //       "throwInternalInconsistencyException_C", fnTy);
  //   this->Builder.CreateCall(fn, {msgVal});
  //   this->Builder.CreateUnreachable();
  // }

  // this->Builder.SetInsertPoint(mergeBB);
  // if (cases.empty()) {
  //   // This would happen if there are NO versions at all, but we checked that
  //   // earlier.
  //   return TypedValue(ObjectTypeSet::dynamicType(),
  //                     PoisonValue::get(types.RT_valueTy));
  // }

  // PHINode *phi =
  //     Builder.CreatePHI(types.RT_valueTy, cases.size(), "dispatch_phi");
  // for (auto &c : cases) {
  //   phi->addIncoming(c.result.value, c.bb);
  // }

  // return TypedValue(ObjectTypeSet::dynamicType(), phi);
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
        if (match)
          cout << "!!!!!!!!!!!! " << id.returnType.toString() << endl;
        if (match)
          return id.returnType;
      }
    }
  }

  return ObjectTypeSet::dynamicType();
}

} // namespace rt
