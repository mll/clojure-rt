#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  for(int i = 0; i< subnode.statements_size(); i++) codegen(subnode.statements(i), ObjectTypeSet::all());

  return codegen(subnode.ret(), typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.ret(), typeRestrictions);
}
