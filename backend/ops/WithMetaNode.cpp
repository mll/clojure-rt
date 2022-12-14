#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions) {
  // TODO We ignore meta for now 
 //   auto meta = codegen(subnode.meta(), ObjectTypeSet::all());
    
    return codegen(subnode.expr(), typeRestrictions);

//  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
//  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const WithMetaNode &subnode, const ObjectTypeSet &typeRestrictions) {
  // TODO We ignore meta for now 
  return getType(subnode.expr(), typeRestrictions);
}

