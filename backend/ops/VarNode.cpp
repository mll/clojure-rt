#include "../codegen.h"  

TypedValue CodeGenerator::codegen(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  if(found == StaticVars.end()) {
    throw CodeGenerationException(string("Undeclared var: ") + name, node);
  }

  Type *t = found->second.first.isDetermined() ? dynamicUnboxedType(found->second.first.determinedType()) : dynamicBoxedType();

  return TypedValue(found->second.first.intersection(typeRestrictions), Builder->CreateLoad(t, found->second.second, "load_var"));
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  if(found != StaticVars.end()) {
    return found->second.first.intersection(typeRestrictions);
  }
  throw CodeGenerationException(string("Undeclared var: ") + name, node);
  return ObjectTypeSet();
}
