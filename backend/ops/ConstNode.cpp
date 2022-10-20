#include "../codegen.h"  

Value *CodeGenerator::codegen(const Node &node, const ConstNode &subnode) {
  cout << "Parsing const node" << endl;
  cout << subnode.type() << endl;
  cout << subnode.val() << endl;
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return nullptr;
}

