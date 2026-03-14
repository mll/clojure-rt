#include "../CodeGen.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const LetNode &subnode,
                            const ObjectTypeSet &typeRestrictions) {
  // We need a new scope for bindings
  variableBindingStack.push();
  variableTypesBindingsStack.push();

  // Guard for resources pushed during binding evaluation
  CleanupChainGuard guard(*this);

  for (int i = 0; i < subnode.bindings_size(); i++) {
    auto bindingNode = subnode.bindings(i);
    auto binding = bindingNode.subnode().binding();

    auto init = codegen(binding.init(), ObjectTypeSet::all());
    
    // We might need to protect the init value if it's a resource
    guard.push(init);

    auto name = binding.name();
    variableBindingStack.set(name, init);
    variableTypesBindingsStack.set(name, init.type);

    // Memory guidance for the binding
    for (const auto &guidance : bindingNode.dropmemory()) {
      memoryManagement.dynamicMemoryGuidance(guidance);
    }
  }

  // Memory guidance for the Let node itself (before body)
  for (const auto &guidance : node.dropmemory()) {
    memoryManagement.dynamicMemoryGuidance(guidance);
  }

  auto retVal = codegen(subnode.body(), typeRestrictions);

  // Pop bindings
  variableBindingStack.pop();
  variableTypesBindingsStack.pop();

  return retVal;
}

ObjectTypeSet CodeGen::getType(const Node &node, const LetNode &subnode,
                               const ObjectTypeSet &typeRestrictions) {
  variableTypesBindingsStack.push();

  for (int i = 0; i < subnode.bindings_size(); i++) {
    auto bindingNode = subnode.bindings(i);
    auto binding = bindingNode.subnode().binding();
    
    auto initType = getType(binding.init(), ObjectTypeSet::all());
    auto name = binding.name();
    variableTypesBindingsStack.set(name, initType);
  }

  auto retVal = getType(subnode.body(), typeRestrictions);
  variableTypesBindingsStack.pop();
  return retVal;
}

} // namespace rt
