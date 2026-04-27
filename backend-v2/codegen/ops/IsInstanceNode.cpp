#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantBool.h"
#include "../../types/ObjectTypeSet.h"
#include "../CleanupChainGuard.h"
#include "../CodeGen.h"
#include "runtime/Class.h"
#include "runtime/ObjectProto.h"
#include <string>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {
TypedValue CodeGen::codegen(const Node &node, const IsInstanceNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);
  TypedValue target = codegen(subnode.target(), ObjectTypeSet::all());
  guard.push(target);

  auto retType = getType(node, typeRestrictions);
  auto constant = retType.getConstant();
  if (constant) {
    memoryManagement.dynamicRelease(target);
    auto boolConst = dynamic_cast<ConstantBoolean *>(constant);
    return TypedValue(retType, Builder.getInt1(boolConst->value));
  }

  // 1. Resolve class name at compile time
  string className = subnode.class_();
  if (className.find("class ") == 0) {
    className = className.substr(6);
  } else if (className.find("interface ") == 0) {
    className = className.substr(10);
  }

  Value *classNameGlobal =
      Builder.CreateGlobalString(className, "is_instance_class");
  Value *jitEngineVal = ConstantInt::get(types.i64Ty, (uintptr_t)jitEnginePtr);
  jitEngineVal = Builder.CreateIntToPtr(jitEngineVal, types.ptrTy);

  FunctionType *isInstSig = FunctionType::get(
      types.i1Ty, {types.ptrTy, types.ptrTy, types.RT_valueTy}, false);
  Value *isInstVal = invokeManager.invokeRaw(
      "Class_isInstanceClassName", isInstSig,
      {classNameGlobal, jitEngineVal, valueEncoder.box(target).value}, &guard);

  return TypedValue(retType, isInstVal);
}

ObjectTypeSet CodeGen::getType(const Node &node, const IsInstanceNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  ObjectTypeSet targetType = getType(subnode.target(), ObjectTypeSet::all());
  if (!targetType.isDetermined()) {
    return ObjectTypeSet(booleanType);
  }
  // 1. Resolve class name
  string className = subnode.class_();
  if (className.find("class ") == 0) {
    className = className.substr(6);
  } else if (className.find("interface ") == 0) {
    className = className.substr(10);
  }
  ::Class *clsRaw =
      this->compilerState.classRegistry.getCurrent(className.c_str());
  if (!clsRaw) {
    clsRaw = this->compilerState.protocolRegistry.getCurrent(className.c_str());
  }
  ScopedRef<::Class> cls(clsRaw);

  if (!cls) {
    throwCodeGenerationException("Class/Protocol " + className + " not found",
                                 node);
  }

  ScopedRef<::Class> target(this->compilerState.classRegistry.getCurrent(
      targetType.determinedType()));
  if (!target) {
    throwCodeGenerationException("Class with registerId '" +
                                     to_string(targetType.determinedType()) +
                                     "' not found",
                                 node);
  }
  return ObjectTypeSet(
      booleanType, false,
      new ConstantBoolean(Class_isInstance(cls.get(), target.get())));
}

} // namespace rt
