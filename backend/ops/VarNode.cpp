#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  if(found == StaticVars.end()) {
    throw CodeGenerationException(string("Undeclared var: ") + name, node);
  }

  auto type = found->second.first;
  Type *t = dynamicType(found->second.first);

  LoadInst * load = Builder->CreateLoad(t, found->second.second, "load_var");
  load->setAtomic(AtomicOrdering::Monotonic);
  if(type.isDynamic()) dynamicRetain(load);

  return TypedValue(found->second.first.restriction(typeRestrictions), load);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  if(found != StaticVars.end()) {
    auto t = found->second.first.restriction(typeRestrictions);
    return t;
  }
  throw CodeGenerationException(string("Undeclared var: ") + name, node);
  return ObjectTypeSet();
}
