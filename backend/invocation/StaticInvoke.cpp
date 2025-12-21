#include "../codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "../cljassert.h"

using namespace std;
using namespace llvm;

extern "C" {
  #include "../runtime/Keyword.h"
}


/* Used by InvokeNode to compile a function during compile time:

     ExitOnErr(TheJIT->addAST(std::move(f)));
     
     and then insert just a direct call code to materialise it without any need of 
     dynamic arg discovery.
*/

TypedValue CodeGenerator::callStaticFun(const Node &node, 
                                        const FnNode& body, 
                                        const pair<FnMethodNode, uint64_t> &method, 
                                        const string &name, 
                                        const ObjectTypeSet &retValType, 
                                        const std::vector<TypedValue> &args, 
                                        const string &refName, 
                                        const TypedValue &callObject, 
                                        std::vector<ObjectTypeSet> &closedOverTypes) {
  vector<Type *> argTypes;
  vector<Value *> argVals;
  vector<ObjectTypeSet> argT;

  vector<TypedValue> finalArgs;

  CLJ_ASSERT((unsigned long)method.first.fixedarity() <= args.size(), "Wrong number of params: received " + to_string(args.size()) + " required " + to_string(method.first.fixedarity()));

  for(int i=0; i<method.first.fixedarity(); i++) {
    auto arg = args[i];
    argTypes.push_back(dynamicType(arg.first));
    argVals.push_back(arg.second);
    argT.push_back(arg.first);
    finalArgs.push_back(arg);
  }
 
  // TODO: For now we just use a vector, in the future a faster sequable data structure will be used here */
  if(method.first.isvariadic()) {
    auto type = ObjectTypeSet(persistentVectorType);
    vector<TypedValue> vecArgs(args.begin() + method.first.fixedarity(), args.end()); 
    argTypes.push_back(dynamicType(type));
    argT.push_back(type);
    auto v = dynamicVector(vecArgs);
    
    argVals.push_back(v);
    finalArgs.push_back(TypedValue(type, v));
  }

//  cout << "Calling static " << argT.size() << endl;


  /* Add closed overs */
  argTypes.push_back(dynamicType(callObject.first));
  argVals.push_back(callObject.second);
  
  string rName = ObjectTypeSet::recursiveMethodKey(name, argT);
  string rqName = ObjectTypeSet::fullyQualifiedMethodKey(name, argT, retValType);
  
  Type *retType = dynamicType(retValType);
  
  Function *CalleeF = TheModule->getFunction(rqName); 
  if(!CalleeF) {
    FunctionType *FT = FunctionType::get(retType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, rqName, TheModule.get());
  }

  auto f = make_unique<FunctionJIT>();
  f->methodIndex = method.second;
  f->args = argT;
  f->retVal = retValType;
  f->uniqueId = getUniqueFunctionIdFromName(name);
  f->closedOvers = closedOverTypes;
  
  ExitOnErr(TheJIT->addAST(std::move(f)));

  auto retVal = TypedValue(retValType, Builder->CreateCall(CalleeF, argVals, string("call_") + rName));  
  if(body.once()) {
    vector<TypedValue> cleanupArgs;
    cleanupArgs.push_back(callObject);    
    callRuntimeFun("Function_cleanupOnce", ObjectTypeSet::empty(), cleanupArgs);       
  }

  return retVal;
}
