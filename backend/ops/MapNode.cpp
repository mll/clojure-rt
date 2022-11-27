#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  vector<TypedValue> args;
  for(int i=0; i<subnode.keys_size(); i++) { 
    args.push_back(codegen(subnode.keys(i), ObjectTypeSet::all()));
    args.push_back(codegen(subnode.vals(i), ObjectTypeSet::all()));
  }
  if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return TypedValue(type, dynamicArrayMap(args));  

  throw CodeGenerationException(string("Compiler does not support the PersistentHashMap pairs yet: ") + Op_Name(node.op()), node);
  return TypedValue(ObjectTypeSet(), nullptr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const MapNode &subnode, const ObjectTypeSet &typeRestrictions) {
  if(subnode.keys_size() <= HASHTABLE_THRESHOLD) return ObjectTypeSet(persistentArrayMapType).restriction(typeRestrictions);
  
  throw CodeGenerationException(string("Compiler does not support the following op yet: ") + Op_Name(node.op()), node);
  return ObjectTypeSet();
}
