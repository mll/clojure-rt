#include "../CodeGen.h"
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
  // Note: Arguments are "consumed" by the call according to the plan.
  std::vector<TypedValue> args;
  CleanupChainGuard argsGuard(*this);
  for (const auto &argNode : subnode.args()) {
    TypedValue t = codegen(argNode, ObjectTypeSet::all());
    args.push_back(t);
    argsGuard.push(t);
  }

  // 3. Generate the invoke call
  // The function object itself is NOT consumed by generateInvoke
  TypedValue result = invokeManager.generateInvoke(fn, args, &argsGuard, &node);

  // 4. Release function object (as it was NOT consumed). Note: this is slow!
  // TODO: can we change function call semantics to have "borrow" for the
  // function object? - this requires a significant rework of the frontend
  // memory pass. Let us check how much this release hurts before improving it.
  memoryManagement.dynamicRelease(fn);

  return TypedValue(result.type.restriction(typeRestrictions), result.value);
}

} // namespace rt
