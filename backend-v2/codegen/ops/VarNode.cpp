#include "../CodeGen.h"
#include "bridge/Exceptions.h"
#include "../../tools/RTValueWrapper.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const VarNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  auto varName = subnode.var().substr(2);
  ScopedRef<Var> var(compilerState.varRegistry.getCurrent(varName.c_str()));
  if (!var)
    throwCodeGenerationException(
        "Unable to resolve var: " + varName + " in this context", node);

  uintptr_t address = reinterpret_cast<uintptr_t>(var.get());
  TypedValue varPtr = TypedValue(
      ObjectTypeSet(varType),
      ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                this->types.ptrTy));
  // Var_deref is a consuming function in C (it calls Ptr_release(self)).
  // We retain it once here, and Var_deref will release it.
  memoryManagement.dynamicRetain(varPtr);

  auto res = invokeManager.invokeRuntime("Var_deref", nullptr,
                                         {ObjectTypeSet(varType)}, {varPtr}, false, nullptr);
  
  return res;
}

ObjectTypeSet CodeGen::getType(const Node &node, const VarNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet::all();
}

} // namespace rt
