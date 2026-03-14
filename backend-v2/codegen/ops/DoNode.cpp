#include "../CodeGen.h"
#include "bytecode.pb.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

TypedValue CodeGen::codegen(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  for (int i = 0; i < subnode.statements_size(); i++) {
    CleanupChainGuard guard(*this);
    auto v = codegen(subnode.statements(i), ObjectTypeSet::all());
    guard.push(v);
    // guard will automatically popResource when it goes out of scope at the end of loop iteration
  }

  return codegen(subnode.ret(), typeRestrictions);
}

ObjectTypeSet CodeGen::getType(const Node &node, const DoNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return getType(subnode.ret(), typeRestrictions);
}

} // namespace rt
