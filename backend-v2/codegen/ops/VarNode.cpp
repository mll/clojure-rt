#include "../CodeGen.h"
#include "bridge/Exceptions.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const VarNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  Var *var = compilerState.varRegistry.getCurrent(varName.c_str());
  if (!var)
    throwCodeGenerationException(
        "Unable to resolve var: " + varName + " in this context", node);

  uintptr_t address = reinterpret_cast<uintptr_t>(var);
  TypedValue varPtr = TypedValue(
      ObjectTypeSet(varType),
      ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                this->types.ptrTy));

  return invokeManager.invokeRuntime("Var_deref", nullptr,
                                     {ObjectTypeSet(varType)}, {varPtr});
}

ObjectTypeSet CodeGen::getType(const Node &node, const VarNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet::all();
}

} // namespace rt
