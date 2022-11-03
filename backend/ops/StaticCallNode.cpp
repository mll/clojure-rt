#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto c = subnode.class_();
  auto m = subnode.method();
  string name;

  if (c.rfind("class ", 0) == 0) {
    name = c.substr(6);
  } else {
    name = c;
  }
  name = name + "/" + m;
  ObjectTypeSet returnTypeMismatched;

  /* We do not want to traverse all the std library here, we should generate candidates and check each of them against the library */
  
  auto found = StaticCallLibrary.find(name);
  if(found == StaticCallLibrary.end()) throw CodeGenerationException(string("Static call ") + name + string(" not implemented"), node);

  auto arities = found->second;

  vector<ObjectTypeSet> types;
  for(int i=0; i< subnode.args_size(); i++) types.push_back(getType(subnode.args(i), ObjectTypeSet::all()));
  string requiredArity = typeStringForArgs(types);
  for(auto method: arities) {
    if(method.first != requiredArity) continue; 
    auto retType = method.second.first;
    if(retType.intersection(typeRestrictions).isEmpty()) continue;
    vector<TypedValue> args;
    
    for(int i=0; i< subnode.args_size();i++) {
      args.push_back(codegen(subnode.args(i), types[i]));
    }
    
    return method.second.second(this, name + " " + requiredArity, node, args);
  }
  throw CodeGenerationException(string("Static call ") + name + string(" not implemented for args: ") + requiredArity + " return type: " + typeRestrictions.toString(), node);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto c = subnode.class_();
  auto m = subnode.method();
  string name;
  
  if (c.rfind("class ", 0) == 0) {
    name = c.substr(6);
  } else {
    name = c;
  }
  name = name + "/" + m;

  auto found = StaticCallLibrary.find(name);
  if(found == StaticCallLibrary.end()) return ObjectTypeSet();

  auto arities = found->second;

  vector<ObjectTypeSet> types;
  for(int i=0; i< subnode.args_size(); i++) types.push_back(getType(subnode.args(i), ObjectTypeSet::all()));
  string requiredArity = typeStringForArgs(types);
  for(auto method: arities) {
    if(method.first != requiredArity) continue; 
    return method.second.first.intersection(typeRestrictions);
  }
  return ObjectTypeSet();
}


