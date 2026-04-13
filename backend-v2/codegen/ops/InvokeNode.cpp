#include "../CodeGen.h"
#include "types/ConstantFunction.h"
#include "types/ObjectTypeSet.h"

namespace rt {

ObjectTypeSet CodeGen::getType(const Node &node, const InvokeNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet::dynamicType().restriction(typeRestrictions);
}

TypedValue CodeGen::codegen(const Node &node, const InvokeNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Evaluate function expression
  TypedValue fn = codegen(subnode.fn(), ObjectTypeSet::all());
  guard.push(fn);

  // 2. Evaluate arguments
  std::vector<TypedValue> args;
  for (const auto &argNode : subnode.args()) {
    TypedValue t = codegen(argNode, ObjectTypeSet::all());
    guard.push(t);
    args.push_back(t);
  }

  // 3. Generate the invoke call
  // The function object itself is NOT consumed by generateInvoke
  TypedValue result;
  if (fn.type.isDetermined() && fn.type.getConstant() &&
      dynamic_cast<ConstantFunction *>(fn.type.getConstant())) {
    result = invokeManager.generateStaticInvoke(fn, args, &guard, &node, {fn});
  } else {
    result = invokeManager.generateDynamicInvoke(fn, args, &guard, &node, {fn});
  }

  // 4. Release function object (as it was NOT consumed). Note: this is slow!
  // TODO: can we change function call semantics to have "borrow" for the
  // function object? - this requires a significant rework of the frontend
  // memory pass. Let us check how much this release hurts before improving it.
  memoryManagement.dynamicRelease(fn);

  return TypedValue(result.type.restriction(typeRestrictions), result.value);
}

} // namespace rt
