#include "../CodeGen.h"
#include "bridge/Module.h"
#include "tools/RTValueWrapper.h"
#include "types/ConstantFunction.h"
#include "types/ObjectTypeSet.h"

using namespace llvm;
using namespace std;

namespace rt {

ObjectTypeSet CodeGen::getType(const Node &node, const InvokeNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet::dynamicType().restriction(typeRestrictions);
}

TypedValue CodeGen::codegen(const Node &node, const InvokeNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  CleanupChainGuard guard(*this);

  // 1. Detect and handle optimized VarCall
  if (subnode.fn().op() == opVar) {
    auto varName = subnode.fn().subnode().var().var().substr(2);
    ScopedRef<Var> var(getOrCreateVar(varName));
    Var *v = var.get();

    uintptr_t address = reinterpret_cast<uintptr_t>(v);
    TypedValue varObj = TypedValue(
        ObjectTypeSet(varType),
        ConstantExpr::getIntToPtr(ConstantInt::get(this->types.i64Ty, address),
                                  this->types.ptrTy));

    // 2. Evaluate arguments
    std::vector<TypedValue> args;
    for (const auto &argNode : subnode.args()) {
      TypedValue t = codegen(argNode, ObjectTypeSet::all());
      guard.push(t);
      args.push_back(t);
    }

    // 3. Generate the VarInvoke call
    TypedValue result =
        invokeManager.generateVarInvoke(varObj, args, &guard, &node);

    return TypedValue(result.type.restriction(typeRestrictions), result.value);
  }

  // STANDARD PATH:
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
  TypedValue result;

  if (fn.type.isDetermined()) {
    switch (fn.type.determinedType()) {
    case keywordType:
      result =
          invokeManager.generateStaticKeywordInvoke(fn, args, &guard, &node);
      break;
    case persistentArrayMapType:
      result = invokeManager.generateStaticMapInvoke(fn, args, &guard, &node);
      break;
    case persistentVectorType:
      result =
          invokeManager.generateStaticVectorInvoke(fn, args, &guard, &node);
      break;
    case functionType: {
      if (dynamic_cast<ConstantFunction *>(fn.type.getConstant())) {
        result =
            invokeManager.generateStaticInvoke(fn, args, &guard, &node, {fn});
      } else {
        result =
            invokeManager.generateDynamicInvoke(fn, args, &guard, &node, {fn});
      }
      break;
    }
    default:
      throwCodeGenerationException(
          "Type " + fn.type.toString() + " is not callable", node);
    }
  } else {
    result = invokeManager.generateDynamicInvoke(fn, args, &guard, &node, {fn});
  }
  return TypedValue(result.type.restriction(typeRestrictions), result.value);
}

} // namespace rt
