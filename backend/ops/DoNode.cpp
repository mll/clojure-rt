#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  for(int i = 0; i< subnode.statements_size(); i++) {
    auto statement = codegen(subnode.statements(i), ObjectTypeSet::all());
    if (!statement.first.isScalar()) dynamicRelease(statement.second, false);
  }

  return codegen(subnode.ret(), typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.ret(), typeRestrictions);
}
