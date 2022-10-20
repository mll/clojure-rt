#include "../codegen.h"  

Value *CodeGenerator::codegen(const Node &node, const CaseTestNode &subnode) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return nullptr;
}

