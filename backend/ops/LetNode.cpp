#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions) {
  std::unordered_map<std::string, TypedValue> bindings;
  std::unordered_map<std::string, ObjectTypeSet> bindingTypes;

  for(int i=0; i < subnode.bindings_size(); i++) {
    auto bindingNode = subnode.bindings(i);
    auto binding = bindingNode.subnode().binding();

    VariableBindingTypesStack.push_back(bindingTypes);
    VariableBindingStack.push_back(bindings);

    codegenDynamicMemoryGuidance(bindingNode);
    auto init = codegen(binding.init(), ObjectTypeSet::all());

    VariableBindingStack.pop_back();
    VariableBindingTypesStack.pop_back();

    auto name = binding.name();
    bindings.insert({name, init});
    bindingTypes.insert({name, init.first});
  } 
  // TODO: memory management
  VariableBindingStack.push_back(bindings);
  VariableBindingTypesStack.push_back(bindingTypes);

  auto retVal = codegen(subnode.body(), typeRestrictions);

  VariableBindingStack.pop_back();
  VariableBindingTypesStack.pop_back();
  return retVal;
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const LetNode &subnode, const ObjectTypeSet &typeRestrictions) {
  std::unordered_map<std::string, ObjectTypeSet> bindings;

  for(int i=0; i < subnode.bindings_size(); i++) {
    auto bindingNode = subnode.bindings(i);
    auto binding = bindingNode.subnode().binding();
    VariableBindingTypesStack.push_back(bindings);
    auto init = getType(binding.init(), ObjectTypeSet::all());
    VariableBindingTypesStack.pop_back();
    auto name = binding.name();
    bindings.insert({name, init});
  } 
  
  VariableBindingTypesStack.push_back(bindings);
  auto retVal = getType(subnode.body(), typeRestrictions);
  VariableBindingTypesStack.pop_back();
  return retVal;
}
