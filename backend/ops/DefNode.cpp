#include "../codegen.h"  


TypedValue CodeGenerator::codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  auto mangled = globalNameForVar(name);

  if(!typeRestrictions.contains(nilType)) throw CodeGenerationException(string("Def cannot be used here because it returns nil: ") + name, node);
  auto nil = dynamicNil();  

  if(found != StaticVars.end()) {
    /* Already processed this def somewhere earlier */
    if(!subnode.has_init()) return TypedValue(ObjectTypeSet(nilType), nil);

    auto foundTypes = found->second.first;
    auto incomingTypes = getType(subnode.init(), ObjectTypeSet::all());
    auto created = codegen(subnode.init(), foundTypes);
    /* TODO - redeclaration requires another global and / or tapping runtime */
    if(created.first.isEmpty()) throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);

    if((foundTypes.isDetermined() && created.first.isDetermined()) ||
       (!foundTypes.isDetermined() && !created.first.isDetermined())) {

      GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
      Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, created.second, MaybeAlign(), AtomicOrdering::Monotonic);
      return TypedValue(ObjectTypeSet(nilType), nil);
    }
    
    if(!foundTypes.isDetermined() && created.first.isDetermined()) {
       GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
       Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, box(created).second, MaybeAlign(), AtomicOrdering::Monotonic);
      return TypedValue(ObjectTypeSet(nilType), nil);
    }

    /* TODO - redeclaration requires another global and tapping runtime */
    throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);
  }
  
  if(!subnode.has_init()) {
    /* First time declaration, empty initialiser */
    TheModule->getOrInsertGlobal(mangled, dynamicBoxedType(nilType));
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
//    gVar->setLinkage(GlobalValue::CommonLinkage);
//    gVar->setAlignment(Align(8));
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, nil, MaybeAlign(), AtomicOrdering::Monotonic);
    StaticVars.insert({name, TypedValue(ObjectTypeSet(nilType), gVar)});
    TheProgramme->StaticVarTypes.insert({name, ObjectTypeSet(nilType)});
  } else {
    /* First time declaration, proper. */

    auto created = codegen(subnode.init(), ObjectTypeSet::all());
    TheModule->getOrInsertGlobal(mangled, created.second->getType());
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    //gVar->setLinkage(GlobalValue::CommonLinkage);
    //gVar->setAlignment(Align(8));


    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, created.second, MaybeAlign(), AtomicOrdering::Monotonic);
    created.second = gVar;
    StaticVars.insert({name, created});
    TheProgramme->StaticVarTypes.insert({name, created.first});
    ConstantFunction *constFun = nullptr;
    if(created.first.isDetermined() && created.first.determinedType() == functionType && (constFun = dynamic_cast<ConstantFunction *>(created.first.getConstant()))) {
      /* Static function declaration, we set some info for the invoke node to speed things up */
      auto found = TheProgramme->StaticFunctions.find(mangled);
      if(found != TheProgramme->StaticFunctions.end()) throw CodeGenerationException(string("Trying to redeclare function that already has static representation, this is compiler programming error"), node);      
      TheProgramme->StaticFunctions.insert({name, constFun->value});
    }
  }

  return TypedValue(ObjectTypeSet(nilType), nil);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
/* TODO var type */
  return ObjectTypeSet(nilType);
}
