#include "../CodeGen.h"
#include "../../bridge/Exceptions.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

  TypedValue CodeGen::codegen(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
    vector<TypedValue> args;
    bool needsGuard = canThrow(node);
    unique_ptr<CleanupChainGuard> guard = needsGuard ? make_unique<CleanupChainGuard>(*this) : nullptr;
    for(int i=0; i<subnode.items_size(); i++) {
      auto v = codegen(subnode.items(i), ObjectTypeSet::all());
      args.push_back(v);
      if (guard) guard->push(v);
    }
    return dynamicConstructor.createVector(args, guard.get());
  }
  
  ObjectTypeSet CodeGen::getType(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
    return ObjectTypeSet(persistentVectorType).restriction(typeRestrictions);
  }

} // namespace rt
