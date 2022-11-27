#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  // TODO: quoting collections
  return codegen(subnode.expr(), typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.expr(), typeRestrictions);
}
