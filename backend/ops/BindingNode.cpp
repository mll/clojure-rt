#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const BindingNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
