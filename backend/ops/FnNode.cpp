#include "../codegen.h"  

pair<objectType, Value *> CodeGenerator::codegen(const Node &node, const FnNode &subnode) {
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return pair<objectType, Value *>(integerType, nullptr);
}

