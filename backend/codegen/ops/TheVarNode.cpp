#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "../../tools/RTValueWrapper.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const TheVarNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  ScopedRef<Var> var(compilerState.varRegistry.getCurrent(varName.c_str()));
  if (!var)
    throwCodeGenerationException(
        "Unable to resolve var: " + varName + " in this context", node);
  uintptr_t address = reinterpret_cast<uintptr_t>(var.get());
  auto retVal = TypedValue(
      ObjectTypeSet(varType),
      ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                this->types.ptrTy));
  memoryManagement.dynamicRetain(retVal);
  return retVal;
}

ObjectTypeSet CodeGen::getType(const Node &node, const TheVarNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(varType);
}

} // namespace rt