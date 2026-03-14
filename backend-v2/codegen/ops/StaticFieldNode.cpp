#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../../tools/EdnParser.h"
#include "../../tools/RTValueWrapper.h"
#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "bytecode.pb.h"
#include "runtime/Object.h"
#include <sstream>

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const StaticFieldNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, subnode, typeRestrictions);

  // 1. If we have a constant value from getType, return it.
  if (type.isDetermined() && type.getConstant()) {
    ObjectTypeConstant *c = type.getConstant();
    if (auto *ci = dynamic_cast<ConstantInteger *>(c)) {
      return this->dynamicConstructor.createInt32(ci->value);
    } else if (auto *cd = dynamic_cast<ConstantDouble *>(c)) {
      return this->dynamicConstructor.createDouble(cd->value);
    } else if (auto *cb = dynamic_cast<ConstantBoolean *>(c)) {
      return this->dynamicConstructor.createBoolean(cb->value);
    }
  }

  // 2. Otherwise, look it up at runtime.
  auto c = subnode.class_();
  auto f = subnode.field();
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

  auto it_field = ext->staticFieldIndices.find(f);
  if (it_field == ext->staticFieldIndices.end()) {
    std::ostringstream oss;
    oss << "Class " << name << " does not have a static field " << f;
    throwCodeGenerationException(oss.str(), node);
  }

  int32_t fieldIndex = it_field->second;

  // We need to pass the ext pointer (void*) and the index (int32)
  Value *extAddr = ConstantInt::get(this->types.i64Ty, (uintptr_t)ext);
  Value *extPtr = Builder.CreateIntToPtr(extAddr, this->types.ptrTy);
  Value *idxVal = ConstantInt::get(this->types.i32Ty, fieldIndex);

  // invokeRuntime unboxed types handling
  std::vector<ObjectTypeSet> pArgTypes = {ObjectTypeSet(classType).unboxed(),
                                          ObjectTypeSet(integerType).unboxed()};
  std::vector<TypedValue> pArgVals = {
      TypedValue(ObjectTypeSet(classType).unboxed(), extPtr),
      TypedValue(ObjectTypeSet(integerType).unboxed(), idxVal)};

  return this->invokeManager.invokeRuntime("ClassExtension_getIndexedStaticField",
                                            &type, pArgTypes, pArgVals);
}

ObjectTypeSet CodeGen::getType(const Node &node, const StaticFieldNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto c = subnode.class_();
  auto f = subnode.field();
  string name = (c.rfind("class ", 0) == 0) ? c.substr(6) : c;

  PtrWrapper<Class> cls(compilerState.classRegistry.getCurrent(name.c_str()));
  if (!cls) {
    return ObjectTypeSet::dynamicType();
  }

  auto *ext = static_cast<ClassDescription *>(cls->compilerExtension);
  if (!ext) {
    return ObjectTypeSet::dynamicType();
  }

  auto it_field = ext->staticFields.find(f);
  if (it_field == ext->staticFields.end()) {
    return ObjectTypeSet::dynamicType();
  }

  RTValue val = it_field->second;
  objectType valType = ::getType(val);
  
  if (valType == integerType) {
    return ObjectTypeSet(integerType, false, new ConstantInteger(RT_unboxInt32(val)));
  } else if (valType == doubleType) {
    return ObjectTypeSet(doubleType, false, new ConstantDouble(RT_unboxDouble(val)));
  } else if (valType == booleanType) {
    return ObjectTypeSet(booleanType, false, new ConstantBoolean(RT_unboxBool(val)));
  }

  return ObjectTypeSet::dynamicType();
}

} // namespace rt
