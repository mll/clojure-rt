#include "../CodeGen.h"
#include "types/ObjectTypeSet.h"

using namespace std;
using namespace llvm;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const WithMetaNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Codegen expression that we want to attach metadata to
  auto expr = codegen(subnode.expr(), ObjectTypeSet::all());
  guard.push(expr);

  // 2. Codegen metadata (must be a map or nil)
  auto meta = codegen(subnode.meta(), ObjectTypeSet::all());
  guard.push(meta);

  // 3. Call specialized withMeta function or RT_withMeta
  TypedValue retVal(expr.type, nullptr);

  if (expr.type.isDetermined()) {
    string fname;
    ObjectTypeSet retType = expr.type;

    switch (expr.type.determinedType()) {
    case symbolType:
      fname = "Symbol_withMeta";
      break;
    case persistentVectorType:
      fname = "PersistentVector_withMeta";
      break;
    case persistentListType:
      fname = "PersistentList_withMeta";
      break;
    case persistentArrayMapType:
      fname = "PersistentArrayMap_withMeta";
      break;
    case varType:
      fname = "Var_resetMeta";
      break;
    default:
      // Type does not support metadata. Release meta and return original value.
      memoryManagement.dynamicRelease(meta);
      return expr;
    }

    retVal = invokeManager.invokeRuntime(
        fname, &retType, {expr.type, ObjectTypeSet::dynamicType()},
        {expr, meta}, false, &guard);
  } else {
    auto dynType = ObjectTypeSet::dynamicType();
    retVal =
        invokeManager.invokeRuntime("RT_withMeta", &dynType, {dynType, dynType},
                                    {expr, meta}, false, &guard);
  }

  return retVal;
}

ObjectTypeSet CodeGen::getType(const Node &node, const WithMetaNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  // Result has the same type as the expression
  return getType(subnode.expr(), typeRestrictions);
}

} // namespace rt
