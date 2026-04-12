#include "../CodeGen.h"

namespace rt {

ObjectTypeSet CodeGen::getType(const Node &node, const InvokeNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  ObjectTypeSet fnType = getType(subnode.fn(), ObjectTypeSet::all());
  std::vector<ObjectTypeSet> argTypes;
  for (const auto &arg : subnode.args()) {
    argTypes.push_back(getType(arg, ObjectTypeSet::all()));
  }

  return invokeManager.predictInvokeType(fnType, argTypes)
      .restriction(typeRestrictions);
}

ObjectTypeSet CodeGen::getType(const Node &node,
                               const KeywordInvokeNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  // TODO - KeywordInvokeNode implementation
  return ObjectTypeSet::all().restriction(typeRestrictions);
}

TypedValue CodeGen::codegen(const Node &node, const KeywordInvokeNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  // TODO - KeywordInvokeNode implementation
  throwCodeGenerationException(
      "Compiler does not support the following op yet: opKeywordInvoke", node);
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
  for (const auto &argNode : subnode.args()) {
    TypedValue t = codegen(argNode, ObjectTypeSet::all());
    args.push_back(t);
    guard.push(t);
  }

  // 3. Generate the invoke call
  // The function object itself is NOT consumed by generateInvoke
  TypedValue result = invokeManager.generateInvoke(fn, args, &guard, &node);

  // 4. Release function object (as it was NOT consumed). Note: this is slow!
  // TODO: can we change function call semantics to have "borrow" for the
  // function object? - this requires a significant rework of the frontend
  // memory pass. Let us check how much this release hurts before improving it.
  memoryManagement.dynamicRelease(fn);

  return TypedValue(result.type.restriction(typeRestrictions), result.value);
}

} // namespace rt
