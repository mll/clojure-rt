#include "../codegen.h"  
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;

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

  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.methods_size(), false)));  
  args.push_back(ConstantInt::get(*TheContext, APInt(64, funId, false)));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.maxfixedarity(), false)));

// We need to add a function pointer for each method in its generic form
// ConstantExpr::getBitCast(MyFunction, Type::getInt8PtrTy(Ctx))

  Value *fun = dynamicCreate(functionType, types, args); 
  types.clear();

  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  
  vector<pair<FnMethodNode, uint64_t>> nodes;
  for(int i=0; i<subnode.methods_size(); i++) {
    auto &method = subnode.methods(i).subnode().fnmethod();
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
    args.clear();
    const FnMethodNode &method = nodes[i].first;
    args.push_back(fun);
    args.push_back(ConstantInt::get(*TheContext, APInt(64, i)));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, nodes[i].second)));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, method.fixedarity())));
    args.push_back(ConstantInt::get(*TheContext, APInt(8, method.isvariadic())));
    args.push_back(Builder->CreateGlobalStringPtr(StringRef(method.loopid().c_str()), "staticString"));
        
    callRuntimeFun("Function_fillMethod", Type::getVoidTy(*TheContext), types, args);
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
    TheProgramme->RecurTargets.insert({firstMethodLoopId, funId});
  } else funId = idEntry->second;

  return ObjectTypeSet(functionType, true, new ConstantFunction(funId)).restriction(typeRestrictions); 
}
