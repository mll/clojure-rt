#include "../codegen.h"  
#include <vector>
#include <algorithm>

TypedValue CodeGenerator::codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  /* TODO - variadic */
  auto function = subnode.fn();
  auto funType = getType(function, ObjectTypeSet::all());
  
  if(funType.isType(functionType) && funType.getConstant()) {
    string name = dynamic_cast<ConstantFunction *>(funType.getConstant())->value;

    FnNode functionBody = Functions.find(name)->second.subnode().fn();  
    vector<TypedValue> args;
    for(int i=0; i< subnode.args_size(); i++) args.push_back(codegen(subnode.args(i), ObjectTypeSet::all()));
    
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
      if(nodes[i]->fixedarity() < args.size() && nodes[i]->isvariadic()) { method = nodes[i]; break;}
    }
    if(method == nullptr) throw CodeGenerationException(string("Function ") + name + " has been called with wrong arity: " + to_string(args.size()), node);

    vector<ObjectTypeSet> argTypes;
    for(int i=0; i<args.size(); i++) argTypes.push_back(args[i].first);

    string rName = recursiveMethodKey(name, argTypes);
    RecursiveFunctionsRetValGuesses.insert({rName, type});
    auto retVal = buildAndCallStaticFun(*method, name, type, args);
    /* We leave the return type cached, maybe in the future it needs to be removed here */
    return retVal;
  }
// TODO: generic functions
  throw CodeGenerationException("We do not support generic functions yet.", node);

  return TypedValue(type, nullptr);
}


ObjectTypeSet CodeGenerator::getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  /* TODO - variadic */
  auto function = subnode.fn();
  auto type = getType(function, ObjectTypeSet::all());
  
  if(type.isType(functionType) && type.getConstant()) {
    string name = dynamic_cast<ConstantFunction *>(type.getConstant())->value;

    FnNode functionBody = Functions.find(name)->second.subnode().fn();  
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
      if(nodes[i]->fixedarity() < args.size() && nodes[i]->isvariadic()) { method = nodes[i]; break;}
    }
    if(method == nullptr) throw CodeGenerationException(string("Function ") + name + " has been called with wrong arity: " + to_string(args.size()), node);
    
    
    string rName = recursiveMethodKey(name, args);
    auto recursiveGuess = RecursiveFunctionsRetValGuesses.find(rName);
    if(recursiveGuess != RecursiveFunctionsRetValGuesses.end()) {
      return recursiveGuess->second;
    }

    auto recursiveName = RecursiveFunctionsNameMap.find(rName);
    if(recursiveName != RecursiveFunctionsNameMap.end()) {
      throw UnaccountedRecursiveFunctionEncounteredException(rName);
    }

    RecursiveFunctionsNameMap.insert({rName, true});    
    
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
        auto inserted = RecursiveFunctionsRetValGuesses.insert({rName, guess});
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

    RecursiveFunctionsNameMap.erase(rName);
    RecursiveFunctionsRetValGuesses.erase(rName);
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
