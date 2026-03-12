#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "types/ObjectTypeSet.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const DefNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  Var *var = getOrCreateVar(varName);
  uint64_t address = reinterpret_cast<uint64_t>(var);
  TypedValue varPtr = TypedValue(
      ObjectTypeSet(varType),
      ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                this->types.ptrTy));
  memoryManagement.dynamicRetain(varPtr);
  if (subnode.has_init()) {
    TypedValue initValue = codegen(subnode.init(), ObjectTypeSet::all());
    memoryManagement.dynamicRetain(varPtr);
    invokeManager.invokeRuntime(
        "Var_bindRoot", nullptr,
        {ObjectTypeSet(varType), ObjectTypeSet::dynamicType()},
        {varPtr, initValue});
  }

  return varPtr;
}

ObjectTypeSet CodeGen::getType(const Node &node, const DefNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  if (!typeRestrictions.contains(varType)) {
    throwCodeGenerationException(
        "Def cannot be used here because it returns var: " + varName, node);
  }
  getOrCreateVar(varName);
  return ObjectTypeSet(varType);
}

} // namespace rt
