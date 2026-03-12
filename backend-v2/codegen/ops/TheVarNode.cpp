#include "../CodeGen.h"
#include "bridge/Exceptions.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const TheVarNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  Var *var = compilerState.varRegistry.getCurrent(varName.c_str());
  if (!var)
    throwCodeGenerationException(
        "Unable to resolve var: " + varName + " in this context", node);
  uintptr_t address = reinterpret_cast<uintptr_t>(var);
  return TypedValue(
      ObjectTypeSet(varType),
      ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                this->types.ptrTy));
}

ObjectTypeSet CodeGen::getType(const Node &node, const TheVarNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(varType);
}

} // namespace rt