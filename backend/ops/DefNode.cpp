#include "../codegen.h"  


TypedValue CodeGenerator::codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  auto mangled = globalNameForVar(name);

  /* TODO - analyse metada for :static and if so block any redefinitions */

  if(!typeRestrictions.contains(nilType)) throw CodeGenerationException(string("Def cannot be used here because it returns nil: ") + name, node);
  auto nil = dynamicNil();  

  if(found != StaticVars.end()) {
    if(!subnode.has_init()) return TypedValue(ObjectTypeSet(nilType), nil);

    auto foundTypes = found->second.first;
    auto incomingTypes = getType(subnode.init(), ObjectTypeSet::all());
    auto created = codegen(subnode.init(), foundTypes);
    /* TODO - redeclaration requires another global and tapping runtime */
    if(created.first.isEmpty()) throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);

    if((foundTypes.isDetermined() && created.first.isDetermined()) ||
       (!foundTypes.isDetermined() && !created.first.isDetermined())) {

      GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
      Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, created.second, MaybeAlign(), AtomicOrdering::Monotonic);
      return TypedValue(ObjectTypeSet(nilType), nil);
    }
    
    if(!foundTypes.isDetermined() && created.first.isDetermined()) {
       GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
       Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, box(created), MaybeAlign(), AtomicOrdering::Monotonic);
      return TypedValue(ObjectTypeSet(nilType), nil);
    }

    /* TODO - redeclaration requires another global and tapping runtime */
    throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);
  }
  
  if(!subnode.has_init()) {
    TheModule->getOrInsertGlobal(mangled, dynamicBoxedType(nilType));
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
//    gVar->setLinkage(GlobalValue::CommonLinkage);
//    gVar->setAlignment(Align(8));
    //gVar->setInitializer();
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, nil, MaybeAlign(), AtomicOrdering::Monotonic);
    StaticVars.insert({name, TypedValue(ObjectTypeSet(nilType), gVar)});
  } else {
    auto created = codegen(subnode.init(), ObjectTypeSet::all());
    TheModule->getOrInsertGlobal(mangled, created.second->getType());
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    //gVar->setLinkage(GlobalValue::CommonLinkage);
    //gVar->setAlignment(Align(8));

//    gVar->setInitializer(created.second);
    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, created.second, MaybeAlign(), AtomicOrdering::Monotonic);
    StaticVars.insert({name, TypedValue(created.first, gVar)});
  }

  return TypedValue(ObjectTypeSet(nilType), nil);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
