#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const StaticCallNode &subnode) {
  auto c = subnode.class_();
  auto m = subnode.method();
  
  vector<TypedValue> args;
  
  for(int i=0; i< subnode.args_size();i++) {
    args.push_back(codegen(subnode.args(i)));
  }

  string name;
  if (c.rfind("class ", 0) == 0) {
    name = c.substr(6);
  } else {
    name = c;
  }
  name = name + "/" + m + " " + typeStringForArgs(args);
  
  auto found = StaticCallLibrary.find(name);
  if (found == StaticCallLibrary.end()) throw CodeGenerationException(string("Static call not implemented: ") + name, node);
  auto fptr = (*found).second;
  return fptr(this, name, node, args);
}

