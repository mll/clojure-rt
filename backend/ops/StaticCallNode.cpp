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

  for(auto it : StaticCallLibrary) {
    auto key = it.first;
    if (key.rfind(name, 0) != 0) continue;
    auto types = typesForArgString(node, key.substr(name.size() + 1, key.size() - name.size() - 1));
    if(subnode.args_size() != types.size()) continue;
    bool argsMatch = true;
    for(int i=0; i<types.size(); i++) {
      auto type = getType(subnode.args(i), ObjectTypeSet(types[i]));
      if(!type.isDetermined()) argsMatch = false;
      /* TODO - dynamic calls? */
    }
    if(!argsMatch) continue;
    
    vector<TypedValue> args;
    
    for(int i=0; i< subnode.args_size();i++) {
      args.push_back(codegen(subnode.args(i), ObjectTypeSet(types[i])));
    }

    auto outType = it.second.first;

    if(typeRestrictions.intersection(outType).isEmpty()) {
      returnTypeMismatched = outType;
      continue;
    }
    auto fptr = it.second.second;
    return fptr(this, key, node, args); 
  }
  
  string types = "";
  for(int i=0; i<types.size(); i++) {
    auto type = getType(subnode.args(i), ObjectTypeSet::all());
    types += type.toString() + ";";
  }

  if(returnTypeMismatched.isEmpty()) throw CodeGenerationException(string("Static call ") + name + string(" not implemented for arg types: ") + types, node);

  throw CodeGenerationException(string("Static call ") + name + string(" Mismatched return type: ") + returnTypeMismatched.toString(), node);

  return TypedValue(ObjectTypeSet(), nullptr);
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
  for(auto it : StaticCallLibrary) {
    auto key = it.first;
    if (key.rfind(name, 0) != 0) continue;
    auto types = typesForArgString(node, key.substr(name.size() + 1, key.size() - name.size() - 1));
    if(subnode.args_size() != types.size()) continue;
    bool argsMatch = true;
    for(int i=0; i<types.size(); i++) {
      auto type = getType(subnode.args(i), ObjectTypeSet(types[i]));
      if(!type.isDetermined()) argsMatch = false;
      /* TODO - dynamic calls? */
    }
    if(!argsMatch) continue;
    auto outType = it.second.first;
    return outType.intersection(typeRestrictions);
  }
  return ObjectTypeSet();
}


