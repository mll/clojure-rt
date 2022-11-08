#include "../codegen.h"  
#include <vector>
#include <algorithm>

TypedValue CodeGenerator::codegen(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions) {
  ObjectTypeSet type = getType(node, typeRestrictions);
  string newName = getMangledUniqueFunctionName();
  Functions.insert({newName, node});
  
  vector<Type *> types;
  vector<Value *> args;
  
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));

  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.methods_size(), false)));  
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(newName.c_str()), "staticString"));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, computeHash(newName.c_str()), false)));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, subnode.maxfixedarity())));

  Value *fun = dynamicCreate(functionType, types, args); 
  types.clear();

  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  
  vector<FnMethodNode> nodes;
  for(int i=0; i<subnode.methods_size(); i++) nodes.push_back(subnode.methods(i).subnode().fnmethod());
  
  std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
  {
    if (lhs.isvariadic() && !rhs.isvariadic()) return false;
    if (!lhs.isvariadic() && rhs.isvariadic()) return true;
    return lhs.fixedarity() > rhs.fixedarity();
  });
  
  for(int i=0; i<subnode.methods_size(); i++) {
    args.clear();
    FnMethodNode method = nodes[i];
    args.push_back(fun);
    args.push_back(ConstantInt::get(*TheContext, APInt(64, i)));
    args.push_back(ConstantInt::get(*TheContext, APInt(64, method.fixedarity())));
    args.push_back(ConstantInt::get(*TheContext, APInt(8, method.isvariadic())));
    args.push_back(Builder->CreateGlobalStringPtr(StringRef(method.loopid().c_str()), "staticString"));

    callRuntimeFun("Function_fillMethod", Type::getVoidTy(*TheContext), types, args);
  } 

  return TypedValue(typeRestrictions.intersection(ObjectTypeSet(functionType)), fun, false, newName);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const FnNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return typeRestrictions.intersection(ObjectTypeSet(functionType)); 
}
