#include "../CodeGen.h"
#include <algorithm>
#include <vector>

namespace rt {

ObjectTypeSet CodeGen::getType(const Node &node, const FnNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  // getType returns the boxed function type without a constant,
  // as implementations are not yet generated.
  return ObjectTypeSet(functionType, false).restriction(typeRestrictions);
}

TypedValue CodeGen::codegen(const Node &node, const FnNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  // 1. Runtime Creation
  // Function_create(methodCount, maxFixedArity, once)
  std::vector<llvm::Value *> createArgs = {
      llvm::ConstantInt::get(types.wordTy, subnode.methods_size()),
      llvm::ConstantInt::get(types.wordTy, subnode.maxfixedarity()),
      Builder.getInt1(subnode.once())};

  llvm::FunctionType *createTy = llvm::FunctionType::get(
      types.ptrTy, {types.wordTy, types.wordTy, types.i1Ty}, false);

  llvm::Value *funObj =
      invokeManager.invokeRaw("Function_create", createTy, createArgs);

  // 2. Sort methods (descending arity, variadic last)
  struct MethodInfo {
    const FnMethodNode *node;
    int originalIndex;
  };
  std::vector<MethodInfo> methods;
  for (int i = 0; i < subnode.methods_size(); ++i) {
    methods.push_back({&subnode.methods(i).subnode().fnmethod(), i});
  }

  std::sort(methods.begin(), methods.end(),
            [](const MethodInfo &a, const MethodInfo &b) {
              if (a.node->isvariadic() != b.node->isvariadic()) {
                return !a.node->isvariadic(); // non-variadic first
              }
              return a.node->fixedarity() <
                     b.node->fixedarity(); // ascending arity
            });

  // 3. Generate baseline implementations and register them
  std::vector<ConstantMethod> constantMethods;

  for (int i = 0; i < (int)methods.size(); ++i) {
    const FnMethodNode &m = *methods[i].node;

    // Calculate types and names for captures (closed-overs)
    std::vector<std::pair<std::string, ObjectTypeSet>> captureInfo;
    for (const auto &node : m.closedovers()) {
      // Calculate type in outer scope.
      // Note: getType() should work correctly here.
      ObjectTypeSet type = getType(node, ObjectTypeSet::all());

      // Determine name
      std::string name = "unknown";
      if (node.subnode().has_local()) {
        name = node.subnode().local().name();
      } else if (node.subnode().has_binding()) {
        name = node.subnode().binding().name();
      }
      captureInfo.push_back({name, type});
    }

    // Generate the IR for the method
    llvm::Function *baselineF = generateBaselineMethod(m, captureInfo);

    // Handle closed overs (captures) for the fillMethod call
    // (these will be boxed RTValues in the runtime)
    std::vector<llvm::Value *> fillArgs;

    // Fixed arguments for Function_fillMethod
    // (self, position, index, fixedArity, isVariadic, implementation, loopId,
    // closedCount, ...captures)
    fillArgs.push_back(funObj);
    fillArgs.push_back(llvm::ConstantInt::get(types.wordTy, i));
    fillArgs.push_back(
        llvm::ConstantInt::get(types.wordTy, methods[i].originalIndex));
    fillArgs.push_back(llvm::ConstantInt::get(types.wordTy, m.fixedarity()));
    fillArgs.push_back(Builder.getInt1(m.isvariadic()));
    fillArgs.push_back(baselineF);
    fillArgs.push_back(
        Builder.CreateGlobalString(m.loopid())); // loopId (string)
    fillArgs.push_back(llvm::ConstantInt::get(types.wordTy, m.closedovers_size()));

    // Codegen and box each capture
    for (int j = 0; j < m.closedovers_size(); ++j) {
      TypedValue capture = codegen(m.closedovers(j), ObjectTypeSet::all());
      TypedValue boxed = valueEncoder.box(capture);
      fillArgs.push_back(boxed.value);
    }

    // Prepare variadic signature for Function_fillMethod
    std::vector<llvm::Type *> fillParamTypes = {
        types.ptrTy, types.wordTy, types.wordTy, types.wordTy,
        types.i1Ty,  types.ptrTy,  types.ptrTy,  types.wordTy};

    // 'true' indicates a variadic function (...)
    llvm::FunctionType *fillTy =
        llvm::FunctionType::get(types.voidTy, fillParamTypes, true);
    invokeManager.invokeRaw("Function_fillMethod", fillTy, fillArgs);

    // Record for ConstantFunction
    constantMethods.push_back(
        {m.fixedarity(), m.isvariadic(), (int)i, baselineF});
  }

  // 4. Return unboxed function object
  ObjectTypeSet enrichedType(
      functionType, false,
      new ConstantFunction(constantMethods, subnode.once()));

  return TypedValue(enrichedType.restriction(typeRestrictions), funObj);
}

} // namespace rt
