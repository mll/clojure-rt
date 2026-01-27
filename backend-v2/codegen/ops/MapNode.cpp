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
    for(int i=0; i<subnode.keys_size(); i++) { 
      keys.push_back(codegen(subnode.keys(i), ObjectTypeSet::all()));
      values.push_back(codegen(subnode.vals(i), ObjectTypeSet::all()));
    }
    if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return dynamicConstructor.createArrayMap(keys, values);  
    
    throwCodeGenerationException(string("Compiler does not support the PersistentHashMap pairs yet: ") + Op_Name(node.op()), node);
    return TypedValue(ObjectTypeSet(), nullptr);
  }
  
  ObjectTypeSet CodeGen::getType(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions) {
    if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return ObjectTypeSet(persistentArrayMapType).restriction(typeRestrictions);
    throwCodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
    return ObjectTypeSet();
  }

} // namespace rt
  
