#include "../CodeGen.h"
#include "bytecode.pb.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  for (int i = 0; i < subnode.statements_size(); i++) {
    auto v = codegen(subnode.statements(i), ObjectTypeSet::all());
    memoryManagement.dynamicRelease(v);
  }

  return codegen(subnode.ret(), typeRestrictions);
}

ObjectTypeSet CodeGen::getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.ret(), typeRestrictions);
}

} // namespace rt
