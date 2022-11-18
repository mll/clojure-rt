#include "../codegen.h"  

using namespace std;
using namespace llvm;

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

  /* We do not want to traverse all the std library here, we should generate candidates and check each of them against the library */
  
  auto found = TheProgramme->StaticCallLibrary.find(name);
  if(found == TheProgramme->StaticCallLibrary.end()) throw CodeGenerationException(string("Static call ") + name + string(" not implemented"), node);

  auto arities = found->second;

  vector<ObjectTypeSet> types;
  for(int i=0; i< subnode.args_size(); i++) types.push_back(getType(subnode.args(i), ObjectTypeSet::all()));
  string requiredArity = typeStringForArgs(types);
  for(auto method: arities) {
    if(method.first != requiredArity) continue; 
    vector<TypedNode> args;
    
    for(int i=0; i< subnode.args_size();i++) {
      args.push_back(TypedNode(types[i], subnode.args(i)));
    }
    
    auto retType = method.second.first(this, name + " " + requiredArity, node, args);
    if(retType.restriction(typeRestrictions).isEmpty()) continue;
    
    return method.second.second(this, name + " " + requiredArity, node, args);
  }
  throw CodeGenerationException(string("Static call ") + name + string(" not implemented for types: ") + requiredArity + "_" + typeStringForArg(typeRestrictions), node);
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

  auto found = TheProgramme->StaticCallLibrary.find(name);
  if(found == TheProgramme->StaticCallLibrary.end()) return ObjectTypeSet();

  auto arities = found->second;

  vector<ObjectTypeSet> types;
  for(int i=0; i< subnode.args_size(); i++) types.push_back(getType(subnode.args(i), ObjectTypeSet::all()));
  string requiredArity = typeStringForArgs(types);
  for(auto method: arities) {
    if(method.first != requiredArity) continue; 
    vector<TypedNode> args;
    
    for(int i=0; i< subnode.args_size();i++) {
      args.push_back(TypedNode(types[i], subnode.args(i)));
    }
    
    return method.second.first(this, name + " " + requiredArity, node, args).restriction(typeRestrictions);
  }
  throw CodeGenerationException(string("Static call ") + name + string(" not implemented for types: ") + requiredArity + "_" + typeStringForArg(typeRestrictions), node);
  return ObjectTypeSet();
}


