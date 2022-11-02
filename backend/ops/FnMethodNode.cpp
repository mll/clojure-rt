#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
