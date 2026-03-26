#include "../../tools/RTValueWrapper.h"
#include "../../types/ConstantBool.h"
#include "../../types/ObjectTypeSet.h"
#include "../CleanupChainGuard.h"
#include "../CodeGen.h"
#include "runtime/Class.h"
#include "runtime/ObjectProto.h"

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
  }
  // 3. Fallback to runtime check
  Value *classNameGlobal = Builder.CreateGlobalString(className, "is_instance_class");
  Value *jitEngineVal = ConstantInt::get(types.i64Ty, (uintptr_t)jitEnginePtr);
  jitEngineVal = Builder.CreateIntToPtr(jitEngineVal, types.ptrTy);
  
  FunctionType *isInstSig = FunctionType::get(types.i1Ty, {types.ptrTy, types.ptrTy, types.RT_valueTy}, false);
  Value *isInstVal = invokeManager.invokeRaw("Class_isInstanceClassName", isInstSig, {classNameGlobal, jitEngineVal, target.value}, &guard);
  
  memoryManagement.dynamicRelease(target);
  return TypedValue(ObjectTypeSet(booleanType, false), isInstVal);
}

ObjectTypeSet CodeGen::getType(const Node &node, const IsInstanceNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  // 1. Resolve class name
  string className = subnode.class_();
  if (className.find("class ") == 0) {
    className = className.substr(6);
  }
  ScopedRef<::Class> cls(
      this->compilerState.classRegistry.getCurrent(className.c_str()));
  if (!cls) {
    throwCodeGenerationException("Class " + className + " not found", node);
  }

  ObjectTypeSet targetType = getType(subnode.target(), ObjectTypeSet::all());

  if (targetType.isDetermined()) {
    return ObjectTypeSet(booleanType, false,
                         new ConstantBoolean(Class_isInstanceObjectType(
                             cls.get(), targetType.determinedType())));
  }
  return ObjectTypeSet(booleanType, false);
}

} // namespace rt
