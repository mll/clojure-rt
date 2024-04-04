#include "../codegen.h"

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string loopId = subnode.loopid();
  std::unordered_map<std::string, TypedValue> bindings;
  std::unordered_map<std::string, ObjectTypeSet> bindingTypes;
  std::vector<TypedValue> bindingVec;
  
  auto loopRetType = getType(node, typeRestrictions);
  auto loopBindingTypes = TheProgramme->LoopsBindingsAndRetValInfers.find(loopId)->second.first;
  
  for (int i = 0; i < subnode.bindings_size(); ++i) {
    auto bindingNode = subnode.bindings(i);
    auto binding = bindingNode.subnode().binding();

    VariableBindingTypesStack.push_back(bindingTypes);
    VariableBindingStack.push_back(bindings);
    
    codegenDynamicMemoryGuidance(bindingNode);
    // Breakpoint here
    auto init = codegen(binding.init(), ObjectTypeSet::all());
    if (!(init.first == loopBindingTypes[i])) init = box(init);
    
    VariableBindingStack.pop_back();
    VariableBindingTypesStack.pop_back();
    
    auto name = binding.name();
    init.first = loopBindingTypes[i];
    bindingVec.push_back(init);
    bindings.insert({name, init});
    bindingTypes.insert({name, init.first});
  }
  
  BasicBlock *currentBB = Builder->GetInsertBlock();
  Function *parentFunction = currentBB->getParent();
  BasicBlock *loopBB = llvm::BasicBlock::Create(*TheContext, loopId + "_entry", parentFunction);
  Builder->CreateBr(loopBB);
  Builder->SetInsertPoint(loopBB);
  
  std::vector<llvm::PHINode *> loopPhis;
  for (auto t: bindingVec) {
    llvm::PHINode *phiNode = Builder->CreatePHI(t.second->getType(), 1, loopId + "_var");
    phiNode->addIncoming(t.second, currentBB);
    loopPhis.push_back(phiNode);
  }
  
  // Code generation in loop body should not use values defined at first loop iteration
  for (int i = 0; i < subnode.bindings_size(); ++i) {
    auto name = subnode.bindings(i).subnode().binding().name();
    bindings.find(name)->second.second = cast<Value>(loopPhis[i]);
  }
  
  // TheProgramme->RecurType.insert({loopId, opLoop});
  LoopInsertPoints.insert({loopId, {loopBB, loopPhis}});
  
  VariableBindingStack.push_back(bindings);
  VariableBindingTypesStack.push_back(bindingTypes);

  auto retVal = codegen(subnode.body(), typeRestrictions);

  VariableBindingStack.pop_back();
  VariableBindingTypesStack.pop_back();
  return retVal;
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const LoopNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string loopId = subnode.loopid();
  TheProgramme->RecurType.insert({loopId, opLoop});
  TheProgramme->LoopsBindingsAndRetValInfers.insert({loopId,
    {{static_cast<std::vector<ObjectTypeSet>::size_type>(subnode.bindings_size()), ObjectTypeSet::empty()}, ObjectTypeSet::empty()}
  });

  while (true) {
    std::unordered_map<std::string, ObjectTypeSet> bindings;
    std::vector<ObjectTypeSet> bindingTypes;

    auto currentLoopTypesIt = TheProgramme->LoopsBindingsAndRetValInfers.find(loopId);
    for (int i = 0; i < subnode.bindings_size(); ++i) {
      auto bindingNode = subnode.bindings(i);
      auto binding = bindingNode.subnode().binding();
      VariableBindingTypesStack.push_back(bindings);
      auto init = getType(binding.init(), ObjectTypeSet::all());
      VariableBindingTypesStack.pop_back();
      currentLoopTypesIt->second.first[i] = currentLoopTypesIt->second.first[i].expansion(init);
      init = currentLoopTypesIt->second.first[i];
      auto name = binding.name();
      bindings.insert({name, init});
      bindingTypes.push_back(init);
    }
    auto oldLoopTypes = currentLoopTypesIt->second; // copy by value before it might modified by inner getType

    VariableBindingTypesStack.push_back(bindings);
    ObjectTypeSet retVal = getType(subnode.body(), typeRestrictions);
    VariableBindingTypesStack.pop_back();
    auto newLoopTypesIt = TheProgramme->LoopsBindingsAndRetValInfers.find(loopId);
    newLoopTypesIt->second.second = retVal;
    if (oldLoopTypes == newLoopTypesIt->second) return retVal;
  }
}
