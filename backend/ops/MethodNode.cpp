#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the node by itself - it always has to be a part of either DefTypeNode or ReifyNode: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const MethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the node by itself - it always has to be a part of either DefTypeNode or ReifyNode: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
