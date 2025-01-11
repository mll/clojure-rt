#include "codegen.h"  
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <sstream>
#include "cljassert.h"

using namespace std;
using namespace llvm;

extern "C" {
  #include "runtime/Keyword.h"
}


/* Called by JIT to build the body of a function. At this stage all arg types are determined */

void CodeGenerator::buildStaticFun(const int64_t uniqueId, 
                                   const uint64_t methodIndex, 
                                   const string &name, 
                                   const ObjectTypeSet &retType, 
                                   const std::vector<ObjectTypeSet> &args, 
                                   std::vector<ObjectTypeSet> &closedOvers) {
  const FnNode &node = TheProgramme->Functions.find(uniqueId)->second.subnode().fn();
  const FnMethodNode &method = node.methods(methodIndex).subnode().fnmethod();
  // cout << "BUILD STATIC " << endl;
  // cout << method.fixedarity() << endl;
  // cout << args.size() << endl;
  // cout << args.back().toString() << endl;
  CLJ_ASSERT((unsigned long)method.params_size() <= args.size(), "Wrong number of params: received " + to_string(args.size()) + " required " + to_string(method.params_size()));

  string rName = ObjectTypeSet::fullyQualifiedMethodKey(name, args, retType);

  vector<Type *> argTypes;
  for(auto arg : args) {
    argTypes.push_back(dynamicType(arg));
  }
  /* Last arg is a pointer to the function object, so that we can extract closed overs */
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());

  Type *retFunType = dynamicType(retType);
  Function *CalleeF = TheModule->getFunction(rName); 

  if(!CalleeF) { 
    FunctionType *FT = FunctionType::get(retFunType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, rName, TheModule.get());
    /* Build body */
    vector<Value *> fArgs;
    for(auto &farg: CalleeF->args()) fArgs.push_back(&farg);

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
    Builder->SetInsertPoint(BB);

    unordered_map<string, ObjectTypeSet> functionArgTypes;
    unordered_map<string, TypedValue> namedFunctionArgs;
    
    const ObjectTypeSet realRetType = determineMethodReturn(method, uniqueId, args, closedOvers, ObjectTypeSet::all());

    if(realRetType == retType || (!retType.isDetermined() && !realRetType.isDetermined())) {      
      for(int i=0; i<method.params_size(); i++) {
        auto name = method.params(i).subnode().binding().name();
        auto value = TypedValue(args[i].removeConst(), fArgs[i]);
        functionArgTypes.insert({name, args[i].removeConst()});      
        namedFunctionArgs.insert({name, value});
      }
      
      Value *functionObject = fArgs[method.params_size()];
      
      if (method.closedovers_size() > 0) {        
        vector<Value *> gepAccess;
        gepAccess.push_back(ConstantInt::get(*TheContext, APInt(64, 0)));
        gepAccess.push_back(ConstantInt::get(*TheContext, APInt(64, methodIndex)));


        Value *methodsPtr = Builder->CreateStructGEP(runtimeFunctionType(), functionObject, 6, "get_methods");
        Value *methodPtr = Builder->CreateGEP(ArrayType::get(runtimeFunctionMethodType(), 0), 
                                              methodsPtr, 
                                              ArrayRef(gepAccess),
                                              "get_method",
                                              true);
        
        Value *closedOversPtr1 = Builder->CreateStructGEP(runtimeFunctionMethodType(), methodPtr, 5, "get_closed_overs");
        auto closedOversPtr = Builder->CreateLoad(dynamicBoxedType(), closedOversPtr1, "load_closed_overs");
        
        for(int i=0; i<method.closedovers_size(); i++) {
          auto name = method.closedovers(i).subnode().local().name();
          auto closedOverType = closedOvers[i];
          if(functionArgTypes.find(name) == functionArgTypes.end()) {
            
            auto closedOverPtr = Builder->CreateGEP(ArrayType::get(dynamicBoxedType(), method.closedovers_size()), 
                                                    closedOversPtr, 
                                                    ArrayRef((Value *)ConstantInt::get(*TheContext, APInt(64, i))),
                                                    "get_closed_over",
                                                    true);
            
            auto closedOverVal = Builder->CreateLoad(dynamicBoxedType(), closedOverPtr, "load_closed_over");
            auto closedOver = TypedValue(closedOverType, closedOverVal);            
            if(!closedOver.first.isBoxedScalar()) dynamicRetain(closedOver.second);
            auto unboxed = unbox(closedOver);
            functionArgTypes.insert({name, unboxed.first});      
            namedFunctionArgs.insert({name, unboxed});
          }
        }
      }
      
      VariableBindingTypesStack.push_back(functionArgTypes);
      VariableBindingStack.push_back(namedFunctionArgs);
      
      /* The actual code generation happens here! */
      
      try {
        auto result = codegen(method.body(), retType);
        // We consume the function object being called (by convention).
        dynamicRelease(functionObject, false);
        Builder->CreateRet(retType.isBoxedScalar() ? box(result).second : result.second);
        verifyFunction(*CalleeF);
      } catch (CodeGenerationException e) {
        CalleeF->eraseFromParent();
        FunctionType *FT = FunctionType::get(retFunType, argTypes, false);
        CalleeF = Function::Create(FT, Function::ExternalLinkage, rName, TheModule.get());
        BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CalleeF);
        Builder->SetInsertPoint(BB);
        runtimeException(e);  
        Builder->CreateRet(dynamicZero(retType));
        verifyFunction(*CalleeF);
      }
      
      VariableBindingStack.pop_back();
      VariableBindingTypesStack.pop_back();  
      
    } else {
      /* This is a case when return type computed from recursion does not match the one 
         that is expected of the function. In this case, we need to call the function with the discovered type and convert the return type afterwards */
      auto f = make_unique<FunctionJIT>();
      f->methodIndex = methodIndex;
      f->args = args;
      f->retVal = realRetType;
      f->uniqueId = uniqueId;
      f->closedOvers = closedOvers;
      ExitOnErr(TheJIT->addAST(std::move(f)));
      

      vector<TypedValue> functionArgs;
      for(int i=0; i<method.params_size(); i++) {
        auto value = TypedValue(args[i].removeConst(), fArgs[i]);
        functionArgs.push_back(value);
      }
      /* Instead of consuming the function object we pass it on */
      functionArgs.push_back(TypedValue(ObjectTypeSet(functionType), fArgs[method.params_size()]));

      TypedValue realRet = callRuntimeFun(ObjectTypeSet::fullyQualifiedMethodKey(name, args, realRetType), realRetType, functionArgs);
      Value *finalRet = nullptr;
      if(realRetType.isDynamic() && retType.isScalar()) 
        finalRet = dynamicUnbox(method.body(), realRet, retType.determinedType()).second;
      if(realRetType.isScalar() && retType.isDynamic()) 
        finalRet = box(realRet).second;
      
      if(realRetType.isDetermined() && retType.isDetermined()) {
        if(realRetType.determinedType() != retType.determinedType()) throw CodeGenerationException("Types mismatch for function call: " + ObjectTypeSet::typeStringForArg(retType) + " " + ObjectTypeSet::typeStringForArg(realRetType), method.body());
        
        if(realRetType.isBoxedScalar()) finalRet = dynamicUnbox(method.body(), realRet, retType.determinedType()).second;
        if(retType.isBoxedScalar()) finalRet = box(realRet).second;
      }
      
      if(finalRet == nullptr) finalRet = realRet.second;
      Builder->CreateRet(finalRet);
      verifyFunction(*CalleeF);
    }
  } // !CalleeF
}
