#include "../codegen.h"  
#include <vector>
#include <algorithm>

using namespace std;
using namespace llvm;


TypedValue CodeGenerator::codegen(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto type = getType(node, typeRestrictions);
  auto functionRef = subnode.fn();
  auto funType = getType(functionRef, ObjectTypeSet::all());
  string fName = "";
  uint64_t uniqueId = 0;
  string refName;

  vector<TypedValue> args;
  for(int i=0; i< subnode.args_size(); i++) {
    auto t = codegen(subnode.args(i), ObjectTypeSet::all());
    args.push_back(TypedValue(t.first.removeConst(), t.second));
  }

  bool determinedArgs = true;
  for(auto a: args) if(!a.first.isDetermined()) { determinedArgs = false; break; }
      
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
  
  if(fName == "" && funType.isBoxedType(functionType) && funType.getConstant()) {
    /* Direct call to a constant function  */
    uniqueId  = dynamic_cast<ConstantFunction *>(funType.getConstant())->value;
    fName = getMangledUniqueFunctionName(uniqueId);
  }
    
  /* All args are determined on compile time, we go static route. This means we build the function 
     with appropriate input types during compile time and just put a call into the node implementation.
       
     In the future we could optimise the method search as method is known when uniqueId > 0 */
  if(uniqueId > 0 && determinedArgs) {
    FnNode functionBody = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();  
  
    /* We need to find a correct method */

    vector<pair<FnMethodNode, uint64_t>> nodes;
    for(int i=0; i<functionBody.methods_size(); i++) {
      auto &method = functionBody.methods(i).subnode().fnmethod();
      nodes.push_back({method, i});
    }
    
    std::sort(nodes.begin(), nodes.end(), [](const auto& lhs, const auto& rhs) -> bool
    {
      if (lhs.first.isvariadic() && !rhs.first.isvariadic()) return false;
      if (!lhs.first.isvariadic() && rhs.first.isvariadic()) return true;
      return lhs.first.fixedarity() > rhs.first.fixedarity();
    });

    int foundIdx = -1;
    for(int i=0; i<nodes.size(); i++) {
      if(nodes[i].first.fixedarity() == args.size()) { foundIdx = i; break;}
      if(nodes[i].first.fixedarity() <= args.size() && nodes[i].first.isvariadic()) { foundIdx = i; break;}
    }
    if(foundIdx == -1) throw CodeGenerationException(string("Function ") + (refName.size() > 0 ? refName : fName) + " has been called with wrong arity: " + to_string(args.size()), node);
    
    auto closedOvers =  TheProgramme->ClosedOverTypes.find(ProgrammeState::closedOverKey(uniqueId, foundIdx))->second;
    for (auto c : closedOvers) if(!c.isDetermined()) { determinedArgs = false; break; }
    /* Short circut - if it turns out that after all closed overs are not determined at compile time we go the dynamic route */

    if (determinedArgs) {
      pair<FnMethodNode, uint64_t> method = nodes[foundIdx];
      
      vector<ObjectTypeSet> argTypes;
      for(int i=0; i<method.first.fixedarity(); i++) argTypes.push_back(args[i].first);
      // TODO: For now we just use a vector, in the future a faster sequable data structure will be used here */
      if(method.first.isvariadic()) argTypes.push_back(ObjectTypeSet(persistentVectorType));
      
      string rName = ObjectTypeSet::recursiveMethodKey(fName, argTypes);
      string rqName = ObjectTypeSet::fullyQualifiedMethodKey(fName, argTypes, type);
      
      
      if(TheModule->getFunction(rqName) == Builder->GetInsertBlock()->getParent()) {
        refName = ""; // This blocks dynamic entry checks - we do not want them for directly recursive functions */
      }
      
      auto callObject = codegen(functionRef, ObjectTypeSet::all());
      auto retVal = callStaticFun(node, functionBody, method, fName, type, args, refName, callObject, closedOvers);
      return retVal;
    }
  }

/* 
  If at least one arg is undetermined - we need to go the dynamic route.
  First, we try to check if we can establish the type of callee during compilation. If so, we can generate much simpler and more taylored code.
*/

  if(funType.isDetermined()) {
    switch(funType.determinedType()) {
    case integerType:
    case doubleType:
    case booleanType:
    case nilType:
    case stringType:
    case bigIntegerType:
    case ratioType:
    case persistentListType:
    case persistentVectorNodeType:
    case concurrentHashMapType:
      throw CodeGenerationException("This type cannot be invoked.", node);
    case symbolType:
      /* Strange clojure behaviour, we just imitate */
      {
        auto funObj = codegen(functionRef, ObjectTypeSet::all());
        dynamicRelease(funObj.second, false);
        return TypedValue(ObjectTypeSet(nilType), dynamicNil());
      }
    case persistentArrayMapType:
      {
        if(args.size() != 1) throw CodeGenerationException("Wrong number of args passed to invokation", node);
        vector<TypedValue> finalArgs;
        string callName;
        finalArgs.push_back(codegen(functionRef, ObjectTypeSet::all()));
        
        finalArgs.push_back(box(args[0]));          
        callName = "PersistentArrayMap_get";        
        return callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs); 
      }
    case keywordType:
      {
        if(args.size() != 1) throw CodeGenerationException("Wrong number of args passed to invokation", node);
        vector<TypedValue> finalArgs;
        string callName;
        finalArgs.push_back(box(args[0]));          
        finalArgs.push_back(codegen(functionRef, ObjectTypeSet::all()));
        
        if(args[0].first.isBoxedType(persistentArrayMapType)) {          
          callName = "PersistentArrayMap_get";
        } else {
          callName = "PersistentArrayMap_dynamic_get";
        }
        return callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs); 
      }
    case functionType:
      /* Function without constant value must be interpreted in the runtime, just like 
         indetermined object */
      break;
    case persistentVectorType:
      { /* TODO: should we move this to a more general function or combine it with static calls?
           we should support big integers here as well */
        if(args.size() != 1) throw CodeGenerationException("Wrong number of args passed to invokation", node);
        vector<TypedValue> finalArgs;
        string callName;
        finalArgs.push_back(codegen(functionRef, ObjectTypeSet::all()));
        auto argType = args[0].first;
        /* Todo - what about big integer? */

        if(args[0].first.isUnboxedType(integerType)) {
          finalArgs.push_back(args[0]);          
          callName = "PersistentVector_nth";
        } else {
          if(!argType.isBoxedType(integerType)) throw CodeGenerationException("The argument must be an integer", node);
          finalArgs.push_back(TypedValue(ObjectTypeSet::dynamicType(), args[0].second));          
          callName = "PersistentVector_dynamic_nth";
        }
        return callRuntimeFun(callName, ObjectTypeSet::dynamicType(), finalArgs); 
      break;
      }
    }
  }
  /* Now we know that what we have at functionRef is either packaged object or a packaged function. 
     we need to go full dynamic here, building dynamic call code with all the complexities.  */
  auto callObject = codegen(functionRef, ObjectTypeSet::all());
  return TypedValue(type, dynamicInvoke(node, callObject.second, getRuntimeObjectType(callObject.second), type, args));    
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const InvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
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

  if(type.isBoxedType(functionType) && type.getConstant()) {
    uniqueId = dynamic_cast<ConstantFunction *>(type.getConstant())->value;
  }

  vector<ObjectTypeSet> args;
  for(int i=0; i< subnode.args_size(); i++) {
    auto t = getType(subnode.args(i), ObjectTypeSet::all());
    args.push_back(t.removeConst());
  }
   
  bool determinedArgs = true;
  for(auto a: args) if(!a.isDetermined()) determinedArgs = false;

  if(uniqueId && determinedArgs) {

    string name = getMangledUniqueFunctionName(uniqueId);
    const FnNode functionBody = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();  

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
    int methodId = -1;
    for(int i=0; i<nodes.size(); i++) {
      if(nodes[i]->fixedarity() == args.size()) { method = nodes[i]; methodId = i; break;}
      if(nodes[i]->fixedarity() <= args.size() && nodes[i]->isvariadic()) { method = nodes[i]; methodId = i; break;}
    }

    if(method == nullptr) throw CodeGenerationException(string("Function ") + name + " has been called with wrong arity: " + to_string(args.size()), node);
    auto closedOvers = TheProgramme->ClosedOverTypes.find(ProgrammeState::closedOverKey(uniqueId, methodId))->second;    
    return determineMethodReturn(*method, uniqueId, args, closedOvers, typeRestrictions);
  }
  /* Unable to find function body, it has gone through generic path and type has to be resolved at runtime */
  return ObjectTypeSet::all().restriction(typeRestrictions);
}
