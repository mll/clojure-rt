#include "../codegen.h"  

using namespace std;
using namespace llvm;

// Reify object captures variables for defined methods in the same way as functions capture variables used by fn-methods

TypedValue CodeGenerator::codegen(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const ReifyNode &subnode, const ObjectTypeSet &typeRestrictions) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
