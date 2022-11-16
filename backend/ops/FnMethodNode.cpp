#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  /* This node appears only in context of compound definitions (DefType, Fn, Reify)  and they have to handle it explicitly (by assuming the contents of their "methods" table are of this type only */
    throw CodeGenerationException(string("Compiler does not support the node by itself - it always has to be a part of either FnNode, DefTypeNode or ReifyNode: ") + Op_Name(node.op()), node);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const FnMethodNode &subnode, const ObjectTypeSet &typeRestrictions) {
  /* This node appears only in context of compound definitions (DefType, Fn, Reify)  and they have to handle it explicitly (by assuming the contents of their "methods" table are of this type only */
    throw CodeGenerationException(string("Compiler does not support the node by itself - it always has to be a part of either FnNode, DefTypeNode or ReifyNode: ") + Op_Name(node.op()), node);
}
