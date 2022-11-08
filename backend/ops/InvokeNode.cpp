#include "../codegen.h"  






TypedValue CodeGenerator::codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  

  





  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {

  // auto funIt = Functions.find()
  
  // if( == Functions.end()
  
  // vector<ObjectTypeSet> argTypes;
  // for(int i=0; i < subnode.args_size(); i++) argTypes.push_back(getType(subnode.args(i), ObjectTypeSet::all())); 





  
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
