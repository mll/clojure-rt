#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return codegen(subnode.expr(), typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return getType(subnode.expr(), typeRestrictions);
}
