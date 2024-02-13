#include "../codegen.h"
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto methodName = subnode.method();
  string className = "vector"; // TODO: How to get that value at runtime?
  
  auto classMethods = TheProgramme->InstanceCallLibrary.find(className);
  if (classMethods == TheProgramme->InstanceCallLibrary.end()) throw CodeGenerationException(string("Class ") + className + string(" not implemented"), node);
  
  auto methods = classMethods->second.find(methodName);
  if (methods == classMethods->second.end()) throw CodeGenerationException(string("Instance call ") + methodName + string(" of class ") + className + string(" not implemented"), node);
  
  vector<ObjectTypeSet> types;
  bool dynamic = false;
  bool foundArity = false;
  
  auto instanceTypes = ObjectTypeSet(persistentVectorType); // based on className
  types.push_back(instanceTypes);
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    types.push_back(t);
    if (!t.isDetermined() || t.isBoxedScalar()) dynamic = true;
  }

  string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
  
  for (auto method: methods->second) {
    auto methodTypes = typesForArgString(node, method.first);
    if (methodTypes.size() == types.size()) foundArity = true;
    if (method.first != requiredTypes) continue;
    
    auto retType = method.second.first(this, methodName + " " + requiredTypes, node, types);
    
    if (retType.restriction(typeRestrictions).isEmpty()) continue;
    
    vector<TypedValue> args;
    
    args.push_back(codegen(subnode.instance(), types[0]));
    for(int i = 0; i < subnode.args_size(); i++) {
       args.push_back(codegen(subnode.args(i), types[i + 1]));
    }
    
    return method.second.second(this, methodName + " " + requiredTypes, node, args);
  }
  
  if (dynamic && foundArity) {
    vector<TypedValue> args;
    
    args.push_back(codegen(subnode.instance(), types[0]));
    for (int i = 0; i < subnode.args_size(); i++) {
       args.push_back(codegen(subnode.args(i), ObjectTypeSet::all()));
    }
    
    vector<set<ObjectTypeSet>> options;
    
    for (int i = 0; i < args.size(); i++) {
      set<ObjectTypeSet> types;
      for (auto method: methods->second) {
        auto methodTypes = typesForArgString(node, method.first);
        if (methodTypes.size() != args.size()) continue;
        types.insert(methodTypes[i]);
      }
      options.push_back(types);
    }
    
    map<vector<ObjectTypeSet>, pair<StaticCallType, StaticCall>> calls;

    for (auto method: methods->second) {
      auto methodTypes = typesForArgString(node, method.first);
      if (methodTypes.size() != args.size()) continue;
      calls.insert({methodTypes, method.second});
    }
    
    BasicBlock *failure = BasicBlock::Create(*TheContext, "failure");
    BasicBlock *merge = BasicBlock::Create(*TheContext, "merge_instance_call");

    Function *parentFunction = Builder->GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&parentFunction->getEntryBlock(),
                     parentFunction->getEntryBlock().begin());
    AllocaInst *retVal = TmpB.CreateAlloca(dynamicBoxedType(), 0, "retVal");

    vector<ObjectTypeSet> path;

    // visitPath(path, Builder->GetInsertBlock(), failure, merge, args, calls, retVal, options, parentFunction, node, this, methodName);
    parentFunction->insert(parentFunction->end(), failure);
    parentFunction->insert(parentFunction->end(), merge);
    Builder->SetInsertPoint(failure);
    runtimeException(CodeGenerationException(string("Instance call ") + methodName + string(" of class ") + className + string(" not implemented for types: ") + requiredTypes + "_" + ObjectTypeSet::typeStringForArg(typeRestrictions), node));
    Builder->CreateUnreachable();
    Builder->SetInsertPoint(merge);
    Value *v = Builder->CreateLoad(dynamicBoxedType(), retVal);
    return TypedValue(ObjectTypeSet::dynamicType(), v);
  }
  
  throw CodeGenerationException(string("Instance call ") + methodName + string(" of class ") + className + string(" not implemented for types: ") + requiredTypes + "_" + ObjectTypeSet::typeStringForArg(typeRestrictions), node);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const InstanceCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string className = "vector";
  auto methodName = subnode.method();
  
  auto classMethods = TheProgramme->InstanceCallLibrary.find(className);
  if (classMethods == TheProgramme->InstanceCallLibrary.end()) return ObjectTypeSet::dynamicType();
  
  auto methods = classMethods->second.find(methodName);
  if (methods == classMethods->second.end()) return ObjectTypeSet::dynamicType();
  
  vector<ObjectTypeSet> types;
  bool dynamic = false;
  bool foundArity = false;
  
  auto instanceTypes = ObjectTypeSet(persistentVectorType); // based on className
  types.push_back(instanceTypes);
  for (int i = 0; i < subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    types.push_back(t);
    if (!t.isDetermined()) dynamic = true;
  }
  
  string requiredTypes = ObjectTypeSet::typeStringForArgs(types);
  
  for (auto method: methods->second) {
    auto methodTypes = typesForArgString(node, method.first);
    if(methodTypes.size() == types.size()) foundArity = true;
    if(method.first != requiredTypes) continue; 
    return method.second.first(this, methodName + " " + requiredTypes, node, types).restriction(typeRestrictions);
  }
  
  if (dynamic && foundArity) return ObjectTypeSet::dynamicType();
  return ObjectTypeSet::all();
}
