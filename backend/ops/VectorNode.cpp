#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  vector<TypedValue> args;
  for(int i=0; i<subnode.items_size(); i++) args.push_back(codegen(subnode.items(i), ObjectTypeSet::all()));

  return TypedValue(type, dynamicVector(args));
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const VectorNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(persistentVectorType).restriction(typeRestrictions);
}
