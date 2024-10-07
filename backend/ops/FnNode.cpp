#include "../codegen.h"  
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;

// Unwind notes: if an exception is thrown during evaluation of fn arguments, previously evaluated values should be dropped
// Example: (f arg1 arg2 arg3) - evaluation LTR - arg2 throws an exception - f and arg1 should be released (by value)

TypedValue CodeGenerator::codegen(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions) {
  ObjectTypeSet type = getType(node, typeRestrictions);
  auto firstMethodLoopId = subnode.methods(0).subnode().fnmethod().loopid();
  auto idEntry = TheProgramme->RecurTargets.find(firstMethodLoopId);
  uint64_t funId = 0;
  if(idEntry == TheProgramme->RecurTargets.end()) {
    funId = getUniqueFunctionId();  
    TheProgramme->Functions.insert({funId, node});
  } else funId = idEntry->second;
  
  vector<Type *> types;
  vector<Value *> args;
  
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext)); // once meta

  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.methods_size(), false)));  
  args.push_back(ConstantInt::get(*TheContext, APInt(64, funId, false)));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.maxfixedarity(), false)));
  args.push_back(ConstantInt::get(*TheContext, APInt(8, 0, false))); // TODO - actual once meta
  

// We need to add a function pointer for each method in its generic form
// ConstantExpr::getBitCast(MyFunction, Type::getInt8PtrTy(Ctx))

  Value *fun = dynamicCreate(functionType, types, args); 

  vector<pair<FnMethodNode, uint64_t>> nodes;
  for(int i=0; i<subnode.methods_size(); i++) {
    auto &method = subnode.methods(i).subnode().fnmethod();
    TheProgramme->RecurType.insert({method.loopid(), opFn});
    TheProgramme->RecurTargets.insert({method.loopid(), funId});
    nodes.push_back({method, i});
  }
  
  std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
  {
    if (lhs.first.isvariadic() && !rhs.first.isvariadic()) return false;
    if (!lhs.first.isvariadic() && rhs.first.isvariadic()) return true;
    return lhs.first.fixedarity() > rhs.first.fixedarity();
  });
  
  for(int i=0; i<subnode.methods_size(); i++) {    
    const FnMethodNode &method = nodes[i].first;
    
    types.clear();

    types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
    types.push_back(Type::getInt64Ty(*TheContext));
    types.push_back(Type::getInt64Ty(*TheContext));
    types.push_back(Type::getInt64Ty(*TheContext));
    types.push_back(Type::getInt8Ty(*TheContext));
    types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo()); // loopId
    types.push_back(Type::getInt64Ty(*TheContext)); // closed overs count
    
    args.clear();
    args.push_back(fun);
    args.push_back(ConstantInt::get(*TheContext, APInt(64, i)));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, nodes[i].second)));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, method.fixedarity())));
    args.push_back(ConstantInt::get(*TheContext, APInt(8, method.isvariadic())));
    args.push_back(Builder->CreateGlobalStringPtr(StringRef(method.loopid().c_str()), "staticString"));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, method.closedovers_size())));
    
    vector<ObjectTypeSet> closedOverTypes;

    for(int j=0; j<method.closedovers_size(); j++) { // closed overs
      auto closedOver = codegen(method.closedovers(j), ObjectTypeSet::all());
      auto boxed = box(closedOver);

      args.push_back(boxed.second);
      closedOverTypes.push_back(boxed.first);
    }

    TheProgramme->ClosedOverTypes.insert({ProgrammeState::closedOverKey(funId, i),
        closedOverTypes});
    
   
    callRuntimeFun("Function_fillMethod", Type::getVoidTy(*TheContext), types, args, true);
  } 

  return TypedValue(type, fun);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto firstMethodLoopId = subnode.methods(0).subnode().fnmethod().loopid();
  auto idEntry = TheProgramme->RecurTargets.find(firstMethodLoopId);
  uint64_t funId = 0;

  if(idEntry == TheProgramme->RecurTargets.end()) {
    funId = getUniqueFunctionId();  
    TheProgramme->Functions.insert({funId, node});
    TheProgramme->RecurType.insert({firstMethodLoopId, opFn});
    TheProgramme->RecurTargets.insert({firstMethodLoopId, funId});
  } else funId = idEntry->second;

  // trigger getType of methods' body, create unbound vars
  for (auto methodNode: subnode.methods()) {
    auto method = methodNode.subnode().fnmethod();
    std::unordered_map<std::string, ObjectTypeSet> bindings;

    for (int i=0; i < method.params_size(); i++) {
      auto paramNode = method.params(i);
      auto binding = paramNode.subnode().binding();
      auto name = binding.name();
      bindings.insert({name, ObjectTypeSet::all()});
    }
    
    VariableBindingTypesStack.push_back(bindings);
    getType(method.body(), typeRestrictions);
    VariableBindingTypesStack.pop_back();
  }
  
  vector<pair<FnMethodNode, uint64_t>> nodes;
  for(int i=0; i<subnode.methods_size(); i++) {
    auto &method = subnode.methods(i).subnode().fnmethod();   
    nodes.push_back({method, i});
  }
  
  std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
  {
    if (lhs.first.isvariadic() && !rhs.first.isvariadic()) return false;
    if (!lhs.first.isvariadic() && rhs.first.isvariadic()) return true;
    return lhs.first.fixedarity() > rhs.first.fixedarity();
  });


  for(int i=0; i<subnode.methods_size(); i++) {    
    const FnMethodNode &method = nodes[i].first;
    vector<ObjectTypeSet> closedOverTypes;

    for(int j=0; j<method.closedovers_size(); j++) { // closed overs
      auto closedOverType = getType(method.closedovers(j), ObjectTypeSet::all());
      auto boxed = closedOverType.boxed();
      closedOverTypes.push_back(boxed);
    }

    TheProgramme->ClosedOverTypes.insert({ProgrammeState::closedOverKey(funId, i),
        closedOverTypes});
  }

  return ObjectTypeSet(functionType, true, new ConstantFunction(funId)).restriction(typeRestrictions); 
}
