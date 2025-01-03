#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions) {
  switch(subnode.local()) {
  case localTypeArg:
  case localTypeLet:
  case localTypeLoop:
    {
      auto name = subnode.name();
      for(int i=VariableBindingStack.size() - 1; i >= 0; i--) {
        auto args = VariableBindingStack[i];
        auto it = args.find(name);
        if(it == args.end()) continue;        
        return it->second;
      }
      throw CodeGenerationException(string("Unknown variable: ") + name, node);         
    }
    break;    
  default:
    // TODO: cases other than args      
      throw CodeGenerationException(string("Compiler does not fully support the following op yet: ") + Op_Name(node.op()) + " Local type: "+ to_string(subnode.local()), node);   
  }
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const LocalNode &subnode, const ObjectTypeSet &typeRestrictions) {
  switch(subnode.local()) {
  case localTypeArg:      
  case localTypeLet:
  case localTypeLoop:
    {
      auto name = subnode.name();
      for(int i=VariableBindingTypesStack.size() - 1; i >= 0; i--) {
        auto args = VariableBindingTypesStack[i];
        auto it = args.find(name);
        if(it == args.end()) continue;
        return it->second;
      }
      throw CodeGenerationException(string("Unknown variable: ") + name, node);   
    }
    break;
  default:
    // TODO: cases other than args      
      throw CodeGenerationException(string("Compiler does not fully support the following op yet: ") + Op_Name(node.op()) + " Local type: "+ to_string(subnode.local()), node);   
  }
}
