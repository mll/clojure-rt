#include "../CodeGen.h"
#include "../../bridge/Exceptions.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

  TypedValue CodeGen::codegen(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
    vector<TypedValue> args;
    for(int i=0; i<subnode.items_size(); i++) args.push_back(codegen(subnode.items(i), ObjectTypeSet::all()));
    return dynamicConstructor.createVector(args);
  }
  
  ObjectTypeSet CodeGen::getType(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
    return ObjectTypeSet(persistentVectorType).restriction(typeRestrictions);
  }

} // namespace rt
