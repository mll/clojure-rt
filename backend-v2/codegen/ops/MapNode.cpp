#include "../CodeGen.h"
#include "../../bridge/Exceptions.h"

using namespace std;
using namespace llvm;
using namespace clojure::rt::protobuf::bytecode;

namespace rt {

  TypedValue CodeGen::codegen(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions) {
    auto type = getType(node, typeRestrictions);
    vector<TypedValue> keys;
    vector<TypedValue> values;    
    bool needsGuard = canThrow(node);
    unique_ptr<ShadowStackGuard> guard = needsGuard ? make_unique<ShadowStackGuard>(*this) : nullptr;
    for(int i=0; i<subnode.keys_size(); i++) { 
      auto k = codegen(subnode.keys(i), ObjectTypeSet::all());
      keys.push_back(k);
      if (guard) guard->push(k);
      auto v = codegen(subnode.vals(i), ObjectTypeSet::all());
      values.push_back(v);
      if (guard) guard->push(v);
    }
    if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return dynamicConstructor.createArrayMap(keys, values, guard.get());  
    
    throwCodeGenerationException(string("Compiler does not support the PersistentHashMap pairs yet: ") + Op_Name(node.op()), node);
    return TypedValue(ObjectTypeSet(), nullptr);
  }
  
  ObjectTypeSet CodeGen::getType(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions) {
    if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return ObjectTypeSet(persistentArrayMapType).restriction(typeRestrictions);
    throwCodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
    return ObjectTypeSet();
  }

} // namespace rt
  
