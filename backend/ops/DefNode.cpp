#include "../codegen.h"  


TypedValue CodeGenerator::codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  auto mangled = globalNameForVar(name);

  if(found != StaticVars.end()) {
    /* TODO - redeclaration requires another global and tappoing runtime */
    throw CodeGenerationException(string("Var already declared: ") + name, node);
  }
  
  if(!typeRestrictions.contains(nilType)) throw CodeGenerationException(string("Def cannot be used here because it returns nil: ") + name, node);

  TheModule->getOrInsertGlobal(mangled, dynamicBoxedType(nilType));
  GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
  gVar->setLinkage(GlobalValue::ExternalLinkage);
  gVar->setAlignment(Align(8));
  auto nil = dynamicNil();
  
  if(!subnode.has_init()) {
      Builder->CreateStore(nil, gVar);
      StaticVars.insert({name, TypedValue(ObjectTypeSet(nilType), gVar)});
  } else {
    auto created = codegen(subnode.init(), ObjectTypeSet::all());
    Builder->CreateStore(created.second, gVar, "store_var");
    StaticVars.insert({name, TypedValue(created.first, gVar)});
  }

  return TypedValue(ObjectTypeSet(nilType), nil);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
