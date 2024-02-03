#include "../codegen.h"  
#include <set>

using namespace std;
using namespace llvm;


/* Memory management is wrong here all along.
   The reason is this:

   Our general convention is, that we 'consume' each non-scalar object during the call.
   Therefore this node needs to make sure the args are released (somehow).

   But, there is also the actual static function implementation.

   Therefore the memory management responsibility is fuzzy here - 
   should the called static function manage it, or should it be managed in this node?

   To add insult to injury, we have situations where the static function 
   (such as clojure.lang.Numbers/add, defined in static/Numbers.cpp)
   has only scalar arguments. In this case, the visitPath function builds a tree of
   conditional statements that decide which version of the static function to call and 
   call it on the unboxed values. In this case, it is the visitPath that *must* release the memory
   as the static function has no idea of memory management. */
  
   

/* visitPath is deep magic - but its purpose is simple - it tries to reuse 
   existing scalar type box in static invokation, such as +, - , etc. */


void visitPath(const vector<ObjectTypeSet> &path, 
               BasicBlock *insertBlock, 
               BasicBlock *failBlock, 
               BasicBlock *mergeBlock, 
               const vector<TypedValue> &args, 
               const map<vector<ObjectTypeSet>, pair<StaticCallType, StaticCall>> & calls, 
               AllocaInst *retVal, 
               const vector<set<ObjectTypeSet>> &options, 
               Function *parentFunction, 
               const Node &node, 
               CodeGenerator *gen, 
               const string &name) {
  gen->Builder->SetInsertPoint(insertBlock);
  string pathString;
  for(auto p : path) pathString+= p.unboxed().toString();

  if(path.size() == args.size()) {
    auto callIt = calls.find(path);
    assert(callIt != calls.end() && "Logic error");
    auto requiredTypes = ObjectTypeSet::typeStringForArgs(callIt->first);
    vector<TypedValue> realArgs;

    for(int i=0; i<args.size(); i++) {
      const TypedValue &arg = args[i];
      if(arg.first.isScalar()) {
        realArgs.push_back(arg);
      } else {
        auto boxedType = callIt->first[i].boxed();
        realArgs.push_back(gen->unbox(TypedValue(boxedType, args[i].second)));
      }
    }

    TypedValue retValForPath = callIt->second.second(gen, name + " " + requiredTypes, node, realArgs);

    // TODO - memory management
    if(retValForPath.first.isScalar()) {
      /* Memory optimisation reuse, as per Renking et al, MSR-TR-2020-42, Nov 29, 2020, v2. */
      Value *potentiallyReusingVar = nullptr;
      for(int i=0; i<args.size(); i++) {
        const TypedValue &arg = args[i];
        const ObjectTypeSet &requiredType = callIt->first[i];        
        if(arg.first.isScalar()) continue;
        /* We try to reuse only potentially scalar types */
        if(!requiredType.isScalar()) continue;
        if(!(requiredType == retValForPath.first)) continue;
        potentiallyReusingVar = arg.second;
        break;
      }

      if(potentiallyReusingVar) {
        auto condValue = gen->dynamicIsReusable(potentiallyReusingVar);
        Function *parentFunction = gen->Builder->GetInsertBlock()->getParent();
        BasicBlock *reuseBB = llvm::BasicBlock::Create(*gen->TheContext, "reuse", parentFunction);  
        BasicBlock *ignoreBB = llvm::BasicBlock::Create(*gen->TheContext, "ignore");
        gen->Builder->CreateCondBr(condValue.second, reuseBB, ignoreBB);
        gen->Builder->SetInsertPoint(reuseBB);
        Value *reusingVar = potentiallyReusingVar;
        for(int i=0; i<args.size(); i++) {
          const TypedValue &arg = args[i];
          if(!arg.first.isScalar() && !(arg.second == reusingVar)) gen->dynamicRelease(arg.second, false);
        }
        /* TODO - why integer here? - it probably doesnt matter as all scalar types have their data stored in the first 8 bytes. But this seems like a hack... */
        StructType *stype = gen->runtimeIntegerType();        
        Value *ptr = gen->Builder->CreateBitOrPointerCast(reusingVar, stype->getPointerTo(), "void_to_struct");
        Value *tPtr = gen->Builder->CreateStructGEP(stype, ptr, 0, "struct_gep");
        
        gen->Builder->CreateStore(retValForPath.second, tPtr, "store_var");
        gen->Builder->CreateStore(reusingVar, retVal);    
        gen->Builder->CreateBr(mergeBlock);
        
        parentFunction->insert(parentFunction->end(), ignoreBB);
        gen->Builder->SetInsertPoint(ignoreBB);
        gen->Builder->CreateStore(gen->box(retValForPath).second, retVal);
        for(int i=0; i<args.size(); i++) {
          const TypedValue &arg = args[i];
          if(!arg.first.isScalar()) gen->dynamicRelease(arg.second, false);
        }
        gen->Builder->CreateBr(mergeBlock);
        return;
      }
    }

    for(int i=0; i<args.size(); i++) {
       const TypedValue &arg = args[i];
       if(!arg.first.isScalar()) gen->dynamicRelease(arg.second, false);
    }
    gen->Builder->CreateStore(gen->box(retValForPath).second, retVal);     
    gen->Builder->CreateBr(mergeBlock);
    return;
  }
  int i = path.size();
  assert(i<options.size());
  auto conds = options[i];
  auto arg = args[i];
  
  Value *computedType = nullptr;
  if(arg.first.isDetermined()) {
    computedType = ConstantInt::get(*(gen->TheContext), APInt(32, arg.first.determinedType(), false));
  } else computedType = gen->getRuntimeObjectType(arg.second);

  SwitchInst *swInst = gen->Builder->CreateSwitch(computedType, failBlock, conds.size());
  vector<pair<BasicBlock *, ObjectTypeSet>> successes;
  assert(conds.size() > 0);
  for(auto cond: conds) {
    BasicBlock *success = BasicBlock::Create(*(gen->TheContext), "success_" + pathString + cond.toString(), parentFunction);
    swInst->addCase(ConstantInt::get(*(gen->TheContext), APInt(32, cond.determinedType(), false)), success);
    successes.push_back({success, cond});
  }
  
  for(int j = 0; j < successes.size(); j++) {
    vector<ObjectTypeSet> newPath = path;
    newPath.push_back(successes[j].second);
    visitPath(newPath, successes[j].first, failBlock, mergeBlock, args, calls, retVal, options, parentFunction, node, gen, name);
  }
}


/****************************** CODEGEN ***************************************************/

TypedValue CodeGenerator::codegen(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto c = subnode.class_();
  auto m = subnode.method();
  string name;

  if (c.rfind("class ", 0) == 0) {
    name = c.substr(6);
  } else {
    name = c;
  }
  name = name + "/" + m;

  auto found = TheProgramme->StaticCallLibrary.find(name);
  if(found == TheProgramme->StaticCallLibrary.end()) throw CodeGenerationException(string("Static call ") + name + string(" not implemented"), node);

  auto methods = found->second;

  bool dynamic = false;
  bool foundArity = false;

  vector<ObjectTypeSet> types;
  for(int i=0; i< subnode.args_size(); i++) types.push_back(getType(subnode.args(i), ObjectTypeSet::all()));

  string requiredTypes = ObjectTypeSet::typeStringForArgs(types);

  /* We push LJLJ towards dynamic invokation, even though it may not be the most efficient way,
     it needs to suffice for now. */

  for(auto t: types) if(!t.isDetermined() || t.isBoxedScalar()) dynamic = true;

  for(auto method: methods) {
    auto methodTypes = typesForArgString(node, method.first);
    if(methodTypes.size() == types.size()) foundArity = true;
    if(method.first != requiredTypes) continue; 

    auto retType = method.second.first(this, name + " " + requiredTypes, node, types);
        
    if(retType.restriction(typeRestrictions).isEmpty()) continue;

    vector<TypedValue> args;
    
    for(int i=0; i< subnode.args_size();i++) {
       args.push_back(codegen(subnode.args(i), types[i]));
    }
    
    return method.second.second(this, name + " " + requiredTypes, node, args);
  }

  if(dynamic && foundArity) { /* Dynamic call, triggered when at least one variable is not fully determined (or boxed scalar) and any method with given arity exists */
    
    vector<TypedValue> args;
    
    for(int i=0; i< subnode.args_size();i++) {
       args.push_back(codegen(subnode.args(i), ObjectTypeSet::all()));
    }

    vector<set<ObjectTypeSet>> options; 
    
    for(int i=0; i<args.size(); i++) {
      set<ObjectTypeSet> types;
      for(auto method: methods) {
        auto methodTypes = typesForArgString(node, method.first);
        if (methodTypes.size() != args.size()) continue;
        types.insert(methodTypes[i]);        
      }
      options.push_back(types);
    }
    
    map<vector<ObjectTypeSet>, pair<StaticCallType,StaticCall>> calls;

    for(auto method: methods) {
      auto methodTypes = typesForArgString(node, method.first);
      if (methodTypes.size() != args.size()) continue;
      calls.insert({methodTypes, method.second});
    }
      
    BasicBlock *failure = BasicBlock::Create(*TheContext, "failure");
    BasicBlock *merge = BasicBlock::Create(*TheContext, "merge_static_call");

    Function *parentFunction = Builder->GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&parentFunction->getEntryBlock(),
                     parentFunction->getEntryBlock().begin());
    AllocaInst *retVal = TmpB.CreateAlloca(dynamicBoxedType(), 0, "retVal");

    vector<ObjectTypeSet> path;

    visitPath(path, Builder->GetInsertBlock(), failure, merge, args, calls, retVal, options, parentFunction, node, this, name);
    parentFunction->insert(parentFunction->end(), failure);
    parentFunction->insert(parentFunction->end(), merge);
    Builder->SetInsertPoint(failure);
    runtimeException(CodeGenerationException(string("Static call ") + name + string(" not implemented for types: ") + requiredTypes + "_" + ObjectTypeSet::typeStringForArg(typeRestrictions), node));
//    Builder->CreateStore(dynamicZero(ObjectTypeSet::dynamicType()), retVal);    
    Builder->CreateUnreachable();    
//    Builder->CreateBr(merge);
    Builder->SetInsertPoint(merge);                 
    Value *v = Builder->CreateLoad(dynamicBoxedType(), retVal);
    return TypedValue(ObjectTypeSet::dynamicType(), v);
  }   /* Dynamic call */
  throw CodeGenerationException(string("Static call ") + name + string(" not implemented for types: ") + requiredTypes + "_" + ObjectTypeSet::typeStringForArg(typeRestrictions), node);
}

/****************************** GET TYPE ***************************************************/

ObjectTypeSet CodeGenerator::getType(const Node &node, const StaticCallNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto c = subnode.class_();
  auto m = subnode.method();
  string name;
  
  if (c.rfind("class ", 0) == 0) {
    name = c.substr(6);
  } else {
    name = c;
  }
  name = name + "/" + m;

  auto found = TheProgramme->StaticCallLibrary.find(name);
  vector<ObjectTypeSet> types;
  bool dynamic = false;
  bool foundArity = false;

  for(int i=0; i< subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    if(!t.isDetermined()) dynamic = true;
    types.push_back(t.unboxed()); /* We always unbox primitives before calling static funs as they never ever implement LJ etc. */
  }

  string requiredTypes = ObjectTypeSet::typeStringForArgs(types);

  if(found == TheProgramme->StaticCallLibrary.end()) {
    throw CodeGenerationException(string("Static call ") + name + string(" not found."), node);
    return ObjectTypeSet();
  }

  auto methods = found->second;

  for(auto method: methods) {
    auto methodTypes = typesForArgString(node, method.first);
    if(methodTypes.size() == types.size()) foundArity = true;
    if(method.first != requiredTypes) continue; 
    return method.second.first(this, name + " " + requiredTypes, node, types).restriction(typeRestrictions);
  }

  if(dynamic && foundArity) return ObjectTypeSet::dynamicType();
  throw CodeGenerationException(string("Static call ") + name + string(" not implemented for types: ") + requiredTypes + "_" + ObjectTypeSet::typeStringForArg(typeRestrictions), node);
  return ObjectTypeSet();
}


