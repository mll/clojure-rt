#include "../codegen.h"  
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;


TypedValue CodeGenerator::codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  /* TODO - variadic */
  auto functionRef = subnode.fn();
  auto funType = getType(functionRef, ObjectTypeSet::all());
  string fName = "";
  uint64_t uniqueId = 0;
  string refName;
  vector<TypedValue> args;
  for(int i=0; i< subnode.args_size(); i++) args.push_back(codegen(subnode.args(i), ObjectTypeSet::all()));
    
  if(functionRef.op() == opVar) { /* Static var holding a fuction */
    auto var = functionRef.subnode().var();
    refName = var.var().substr(2);
    auto mangled = globalNameForVar(refName);
    auto idIt = TheProgramme->StaticFunctions.find(mangled);
    if(idIt != TheProgramme->StaticFunctions.end()) {
      uniqueId = idIt->second;
      fName = getMangledUniqueFunctionName(uniqueId);
    }
  }
  
  if(fName == "" && funType.isType(functionType) && funType.getConstant()) {
    /* Direct call to a constant function  */
    uniqueId  = dynamic_cast<ConstantFunction *>(funType.getConstant())->value;
    fName = getMangledUniqueFunctionName(uniqueId);
  }

  if(uniqueId > 0) {
    FnNode functionBody = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();  
  
    /* We need to find a correct method */

    vector<const FnMethodNode *> nodes;
    for(int i=0; i<functionBody.methods_size(); i++) nodes.push_back(&(functionBody.methods(i).subnode().fnmethod()));
    std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
    {
      if (lhs->isvariadic() && !rhs->isvariadic()) return false;
      if (!lhs->isvariadic() && rhs->isvariadic()) return true;
      return lhs->fixedarity() > rhs->fixedarity();
    });

    const FnMethodNode *method = nullptr;
    for(int i=0; i<nodes.size(); i++) {
      if(nodes[i]->fixedarity() == args.size()) { method = nodes[i]; break;}
      if(nodes[i]->fixedarity() <= args.size() && nodes[i]->isvariadic()) { method = nodes[i]; break;}
    }
    if(method == nullptr) throw CodeGenerationException(string("Function ") + (refName.size() > 0 ? refName : fName) + " has been called with wrong arity: " + to_string(args.size()), node);

    vector<ObjectTypeSet> argTypes;
    for(int i=0; i<args.size(); i++) argTypes.push_back(args[i].first);

    string rName = recursiveMethodKey(fName, argTypes);
    
    
    if(TheModule->getFunction(rName) == Builder->GetInsertBlock()->getParent()) {
//      refName = ""; // This blocks dynamic entry checks - we do not want them for directly recursive functions */
    }

    auto retVal = callStaticFun(*method, fName, type, args, refName);
    /* We leave the return type cached, maybe in the future it needs to be removed here */
    return retVal;
  }

  if(funType.isDetermined()) {
    switch(funType.determinedType()) {
    case integerType:
    case doubleType:
    case booleanType:
    case nilType:
    case stringType:
    case persistentListType:
    case persistentVectorNodeType:
    case concurrentHashMapType:
      throw CodeGenerationException("This type cannot be invoked.", node);
    case symbolType:
      /* Strange clojure behaviour, we just imitate */
      return TypedValue(ObjectTypeSet(nilType), dynamicNil());
    case keywordType:
      /* TODO */
      throw CodeGenerationException("We do not support keyword invoke yet.", node);
    case functionType:
      /* Function without constant value must be interpreted in the runtime, just like 
         indetermined object */
      break;
    case persistentVectorType:
      { /* TODO: should we move this to a more general function or combine it with static calls?
           we should support big integers here as well */
        vector<TypedValue> args;
        for(int i=0; i< subnode.args_size(); i++) args.push_back(codegen(subnode.args(i), ObjectTypeSet::all()));
        if(args.size() != 1) throw CodeGenerationException("Wrong number of args passed to invokation", node);
        vector<TypedValue> finalArgs;
        string callName;
        finalArgs.push_back(codegen(functionRef, ObjectTypeSet::all()));
        auto argType = args[0].first;
        /* Todo - what about big integer? */
        if(argType.isDetermined() && !argType.isType(integerType)) throw CodeGenerationException("The argument must be an integer", node);
        
        if(args[0].first.isType(integerType)) {
          finalArgs.push_back(args[0]);          
          callName = "PersistentVector_nth";
        } else {
          finalArgs.push_back(TypedValue(ObjectTypeSet::dynamicType(), args[0].second));          
          callName = "PersistentVector_dynamic_nth";
        }
        return callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs); 
      break;
      }
    }
  }
  
  
  

  
// TODO: generic functions
// ConstantExpr::getBitCast(MyFunction, Type::getInt8PtrTy(Ctx))
  throw CodeGenerationException("We do not support generic functions yet.", node);

  return TypedValue(type, nullptr);
}


ObjectTypeSet CodeGenerator::getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  /* TODO - variadic */
  auto function = subnode.fn();
  auto type = getType(function, ObjectTypeSet::all());
  uint64_t uniqueId = 0;

    
  if(function.op() == opVar) { /* Static var holding a fuction */
    auto var = function.subnode().var();
    string name = var.var().substr(2);
    auto mangled = globalNameForVar(name);
    auto idIt = TheProgramme->StaticFunctions.find(mangled);
    if(idIt != TheProgramme->StaticFunctions.end()) {
      uniqueId = idIt->second;
    }
  }

  if(type.isType(functionType) && type.getConstant()) uniqueId = dynamic_cast<ConstantFunction *>(type.getConstant())->value;
  
  if(uniqueId) {

    string name = getMangledUniqueFunctionName(uniqueId);
    const FnNode functionBody = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();  

    vector<ObjectTypeSet> args;
    for(int i=0; i< subnode.args_size(); i++) args.push_back(getType(subnode.args(i), ObjectTypeSet::all()));
    
    /* We need to find a correct method */

    vector<const FnMethodNode *> nodes;
    for(int i=0; i<functionBody.methods_size(); i++) nodes.push_back(&(functionBody.methods(i).subnode().fnmethod()));
    std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
    {
      if (lhs->isvariadic() && !rhs->isvariadic()) return false;
      if (!lhs->isvariadic() && rhs->isvariadic()) return true;
      return lhs->fixedarity() > rhs->fixedarity();
    });

    const FnMethodNode *method = nullptr;
    for(int i=0; i<nodes.size(); i++) {
      if(nodes[i]->fixedarity() == args.size()) { method = nodes[i]; break;}
      if(nodes[i]->fixedarity() <= args.size() && nodes[i]->isvariadic()) { method = nodes[i]; break;}
    }

    if(method == nullptr) throw CodeGenerationException(string("Function ") + name + " has been called with wrong arity: " + to_string(args.size()), node);
    
    
    string rName = recursiveMethodKey(name, args);
    auto recursiveGuess = TheProgramme->RecursiveFunctionsRetValGuesses.find(rName);
    if(recursiveGuess != TheProgramme->RecursiveFunctionsRetValGuesses.end()) {
      return recursiveGuess->second;
    }

    auto recursiveName = TheProgramme->RecursiveFunctionsNameMap.find(rName);
    if(recursiveName != TheProgramme->RecursiveFunctionsNameMap.end()) {
      throw UnaccountedRecursiveFunctionEncounteredException(rName);
    }

    TheProgramme->RecursiveFunctionsNameMap.insert({rName, true});    
    
    FunctionArgTypesStack.push_back(args);
    ObjectTypeSet retVal;
    bool found = false;
    vector<CodeGenerationException> exceptions;
    try {
      retVal = getType(method->body(), typeRestrictions);
      if(!retVal.isEmpty()) found = true;
    } catch(UnaccountedRecursiveFunctionEncounteredException e) {
      if(e.functionName != rName) throw e;
      auto guesses = ObjectTypeSet::allGuesses();
      for(auto guess : guesses) {
        auto inserted = TheProgramme->RecursiveFunctionsRetValGuesses.insert({rName, guess});
        inserted.first->second = guess;
        try {
          retVal = getType(method->body(), typeRestrictions);
          if(!retVal.isEmpty()) found = true;
        } catch(CodeGenerationException e) {
          exceptions.push_back(e);
        }
        if(found) break;
      }
    }
    FunctionArgTypesStack.pop_back();

    TheProgramme->RecursiveFunctionsNameMap.erase(rName);
    TheProgramme->RecursiveFunctionsRetValGuesses.erase(rName);
    // TODO: better error here, maybe use the exceptions vector? 
    if(!found) {
      for(auto e : exceptions) cout << e.toString() << endl;
      throw CodeGenerationException("Unable to create function with given params", node);
    }
    return retVal;
  }
  /* Unable to find function body, it has gone through generic path and type has to be resolved at runtime */
  return ObjectTypeSet::all();
}
