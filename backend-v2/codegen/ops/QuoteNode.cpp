#include "../CodeGen.h"
#include "../../bridge/Exceptions.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  // TODO: quoting collections
  return codegen(subnode.expr(), typeRestrictions);
}

ObjectTypeSet CodeGen::getType(const Node &node, const QuoteNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.expr(), typeRestrictions);
}

} // namespace rt
