#include "../codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "../cljassert.h"

using namespace std;
using namespace llvm;

extern "C" {
  #include "../runtime/Keyword.h"
}

ObjectTypeSet CodeGenerator::determineMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions) {
  string name = getMangledUniqueFunctionName(uniqueId);
  string rName = ObjectTypeSet::recursiveMethodKey(name, args);
  auto functionInference = TheProgramme->FunctionsRetValInfers.find(rName);

  if (functionInference == TheProgramme->FunctionsRetValInfers.end()) {
    TheProgramme->FunctionsRetValInfers.insert({rName, ObjectTypeSet()});
  } else {
    return functionInference->second;
  }
  
  auto returnType = ObjectTypeSet(), newReturnType = ObjectTypeSet();
  do {
    returnType = newReturnType;
    newReturnType = inferMethodReturn(method, uniqueId, args, closedOvers, typeRestrictions);
    TheProgramme->FunctionsRetValInfers.insert_or_assign(rName, newReturnType);
  } while (!(returnType == newReturnType));
  
  return newReturnType;
}

ObjectTypeSet CodeGenerator::inferMethodReturn(const FnMethodNode &method, const uint64_t uniqueId, const vector<ObjectTypeSet> &args, const std::vector<ObjectTypeSet> &closedOvers, const ObjectTypeSet &typeRestrictions) {
    unordered_map<string, ObjectTypeSet> namedArgs;

    for(int i=0; i<method.fixedarity(); i++) {
      auto name = method.params(i).subnode().binding().name();
      namedArgs.insert({name, args[i]});
    }

    if (method.isvariadic()) {
      string name;
      if (method.params_size() == method.fixedarity()) name = "***unbound-variadic***";
      else name = method.params(method.fixedarity()).subnode().binding().name();
      namedArgs.insert({name, ObjectTypeSet(persistentVectorType)});
    }
    
    for(int i=0; i<method.closedovers_size(); i++) {
      auto name = method.closedovers(i).subnode().local().name();
      if(namedArgs.find(name) == namedArgs.end()) {
        namedArgs.insert({name, closedOvers[i].unboxed()});  
      }
    }

    VariableBindingTypesStack.push_back(namedArgs);
    ObjectTypeSet retVal;
    vector<CodeGenerationException> exceptions;
    try {
      retVal = getType(method.body(), typeRestrictions);
    } catch(CodeGenerationException e) {
      exceptions.push_back(e);
    }

    VariableBindingTypesStack.pop_back();

    // TODO: better error here, maybe use the exceptions vector? 
    if(retVal.isEmpty()) {
      std::string exceptionString;
      for(auto e : exceptions) exceptionString += e.toString() + "\n";
      throw CodeGenerationException("Unable to create function with given params: "+ exceptionString , method.body());
    }
    return retVal;
} 

