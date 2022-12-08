#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = StaticVars.find(name);
  auto mangled = globalNameForVar(name);

  if(!typeRestrictions.contains(nilType)) throw CodeGenerationException(string("Def cannot be used here because it returns nil: ") + name, node);
  auto nil = dynamicNil();  

  if(found != StaticVars.end()) {
    /* Already processed this def somewhere earlier */
    /* TODO - undefing */
    if(!subnode.has_init()) return TypedValue(ObjectTypeSet(nilType), nil);
/* Hint: commenting the line below allows for easy "multithreaded" debugging of function entry shifts */
    TheProgramme->StaticFunctions.erase(mangled);
    auto foundTypes = found->second.first;
    auto incomingTypes = getType(subnode.init(), ObjectTypeSet::all());
    auto created = codegen(subnode.init(), foundTypes);
    /* TODO - redeclaration requires another global and / or tapping runtime */
    if(created.first.isEmpty()) throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);

    TypedValue newValue;
 
    if((foundTypes.isDetermined() && created.first.isDetermined()) ||
       (!foundTypes.isDetermined() && !created.first.isDetermined())) {
      newValue = created;
    } else if(!foundTypes.isDetermined() && created.first.isDetermined()) {
      newValue = box(created);
    } else {
      /* TODO - redeclaration requires another global and tapping runtime */
      throw CodeGenerationException(string("Var type change impossible at this time: ") + name + " type: " + foundTypes.toString() + " incoming types: " + incomingTypes.toString(), node);
    }
    
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
    Type *type = dynamicType(newValue.first);

    LoadInst * load = Builder->CreateLoad(type, gVar, "load_var");
    load->setAtomic(AtomicOrdering::Monotonic);

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *PreheaderBB = Builder->GetInsertBlock();
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);

    // Insert an explicit fall through from the current block to the LoopBB.
    Builder->CreateBr(LoopBB);
    Builder->SetInsertPoint(LoopBB);

    PHINode *Variable = Builder->CreatePHI(type, 2, "cmp_var");
    Variable->addIncoming(load, PreheaderBB);

    Value *pair = Builder->CreateAtomicCmpXchg(gVar, load, newValue.second, MaybeAlign(), AtomicOrdering::Monotonic,  AtomicOrdering::Monotonic);
    
    Value *success = Builder->CreateExtractValue(pair, 1, "success");
    Value *newLoaded = Builder->CreateExtractValue(pair, 0, "newloaded");    
    
    BasicBlock *LoopEndBB = Builder->GetInsertBlock();
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    Builder->CreateCondBr(success, AfterBB, LoopBB);

    Builder->SetInsertPoint(AfterBB);
    Variable->addIncoming(newLoaded, LoopEndBB);

    if(!newValue.first.isScalar()) dynamicRelease(Variable, true);
    
    vector<Value *> rtArgs;
    vector<Type *> rtArgTs;

    Value* jitAddressConst = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (int64_t)TheJIT, false));
    Value* jitAddress = Builder->CreateIntToPtr(jitAddressConst, Type::getInt8Ty(*TheContext)->getPointerTo(), "int_to_ptr"); 

    rtArgs.push_back(jitAddress);
    rtArgs.push_back(newValue.second);
    rtArgs.push_back(Builder->CreateGlobalStringPtr(StringRef(name), "staticString"));
    
    rtArgTs.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
    rtArgTs.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
    rtArgTs.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());

    callRuntimeFun("setFunForVar", Type::getVoidTy(*TheContext), rtArgTs, rtArgs, false);

    return TypedValue(ObjectTypeSet(nilType), nil);
  }
  
  if(!subnode.has_init()) {
    /* First time declaration, empty initialiser */
    TheModule->getOrInsertGlobal(mangled, dynamicBoxedType(nilType));
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
//    gVar->setLinkage(GlobalValue::CommonLinkage);
//    gVar->setAlignment(Align(8));
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, nil, MaybeAlign(), AtomicOrdering::Monotonic);
    StaticVars.insert({name, TypedValue(ObjectTypeSet::dynamicType(), gVar)});
    TheProgramme->StaticVarTypes.insert({name, ObjectTypeSet::dynamicType()});
  } else {
    /* First time declaration, proper. */

    auto created = box(codegen(subnode.init(), ObjectTypeSet::all()));
    TheModule->getOrInsertGlobal(mangled, created.second->getType());
    GlobalVariable *gVar = TheModule->getNamedGlobal(mangled);
    gVar->setInitializer(Constant::getNullValue(gVar->getType()));
    //gVar->setLinkage(GlobalValue::CommonLinkage);
    //gVar->setAlignment(Align(8));

    ConstantFunction *constFun = nullptr;
    if(created.first.isDetermined() && created.first.determinedType() == functionType && (constFun = dynamic_cast<ConstantFunction *>(created.first.getConstant()))) {
      /* Static function declaration, we set some info for the invoke node to speed things up. This works only the first time the function is declared under this var. */
      TheProgramme->StaticFunctions.insert({mangled, constFun->value});
    }

    Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, gVar, created.second, MaybeAlign(), AtomicOrdering::Monotonic);
    created.second = gVar;
    created.first = created.first.removeConst();
    StaticVars.insert({name, TypedValue(ObjectTypeSet::dynamicType(), created.second)});
    TheProgramme->StaticVarTypes.insert({name, ObjectTypeSet::dynamicType()});

  }

  return TypedValue(ObjectTypeSet(nilType), nil);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
/* TODO var type */
  return ObjectTypeSet(nilType);
}
