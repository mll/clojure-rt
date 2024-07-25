#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto retType = getType(node, subnode, typeRestrictions);
  Value *name = dynamicString(subnode.name().c_str());
  dynamicRetain(name);
  auto classNameStr = subnode.classname(); // contains "class " prefix
  Value *className = dynamicString(classNameStr.c_str());
  dynamicRetain(className);
  auto superclassStr = subnode.superclass();
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  
  Value *superclassPtr;
  if (superclassStr == "") {
    superclassPtr = Constant::getNullValue(ptrT);
  } else {
    Function *parentFunction = Builder->GetInsertBlock()->getParent();
    Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
    Value *superclassName = dynamicString(superclassStr.c_str());
    dynamicRetain(superclassName);
    superclassPtr = callRuntimeFun("getClassByName", ptrT, {ptrT, ptrT}, {statePtr, superclassName});
    BasicBlock *classNotFoundBB = llvm::BasicBlock::Create(*TheContext, "class_not_found", parentFunction);
    BasicBlock *classFoundBB = llvm::BasicBlock::Create(*TheContext, "class_found", parentFunction);
    Value *superclassNotFound = Builder->CreateICmpEQ(superclassPtr, Constant::getNullValue(ptrT));
    Builder->CreateCondBr(superclassNotFound, classNotFoundBB, classFoundBB);
    Builder->SetInsertPoint(classNotFoundBB);
    runtimeException(CodeGenerationException("Class not found: " + superclassStr, node)); 
    Builder->SetInsertPoint(classFoundBB);
  }
  
  Value *flags = ConstantInt::get(*TheContext, APInt(64, 0, false)); // no flags for now
  
  // TODO: Static fields in deftype, in metadata?
  Value *staticFieldCount = ConstantInt::get(*TheContext, APInt(64, 0, false));
  Value *staticFieldNames = Constant::getNullValue(ptrT);
  Value *staticFields = Constant::getNullValue(ptrT);
  
  // TODO: Static methods in deftype, in metadata?
  Value *staticMethodCount = ConstantInt::get(*TheContext, APInt(64, 0, false));
  Value *staticMethodNames = Constant::getNullValue(ptrT);
  Value *staticMethods = Constant::getNullValue(ptrT);
  
  Value *fieldCount = ConstantInt::get(*TheContext, APInt(64, subnode.fields().size(), false));
  std::vector<Type *> fieldTypes {Type::getInt64Ty(*TheContext)};
  std::vector<Value *> fieldArgs {fieldCount};
  for (auto field: subnode.fields()) {
    fieldTypes.push_back(ptrT);
    fieldArgs.push_back(dynamicKeyword(field.subnode().binding().name().c_str()));
  }
  Value *fieldNames = callRuntimeFun("packPointerArgs", ptrT, fieldTypes, fieldArgs, true);
  
  // Group class methods by name, check that their arities don't conflict, create a function
  unordered_map<string, vector<MethodNode>> methodFunctionMethods;
  for (auto methodNode: subnode.methods()) {
    auto method = methodNode.subnode().method();
    auto methodIt = methodFunctionMethods.find(method.name());
    if (methodIt == methodFunctionMethods.end()) {
      methodFunctionMethods.insert({method.name(), {method}});
    } else {
      methodIt->second.push_back(method);
    }
  }
  // Group class methods: verification
  for (auto methodFunctionMethod: methodFunctionMethods) {
    std::sort(methodFunctionMethod.second.begin(), methodFunctionMethod.second.end(), [](const MethodNode& lhs, const MethodNode& rhs) -> bool
    {
      bool lhsIsVariadic = lhs.params().size() > lhs.fixedarity();
      bool rhsIsVariadic = rhs.params().size() > rhs.fixedarity();
      if (lhsIsVariadic && !rhsIsVariadic) return false;
      if (!lhsIsVariadic && rhsIsVariadic) return true;
      return lhs.fixedarity() > rhs.fixedarity();
    });
    for (uint64_t i = 0; i < methodFunctionMethod.second.size() - 1; ++i) {
      // Two methods with the same arity
      if (methodFunctionMethod.second[i].fixedarity() == methodFunctionMethod.second[i + 1].fixedarity()) {
        runtimeException(CodeGenerationException("Two methods with the same arity " + to_string(methodFunctionMethod.second[i].fixedarity()) + " in " + methodFunctionMethod.first, node));
        return TypedValue(ObjectTypeSet(nilType), dynamicNil());
      }
      // Two variadic methods
      if (methodFunctionMethod.second.size() > 1) {
        auto fnMethod = methodFunctionMethod.second[methodFunctionMethod.second.size() - 2];
        if (fnMethod.params().size() > fnMethod.fixedarity()) {
          runtimeException(CodeGenerationException("Two variadic methods " + to_string(methodFunctionMethod.second[i].fixedarity()) + " in " + methodFunctionMethod.first, node));
          return TypedValue(ObjectTypeSet(nilType), dynamicNil());
        }
      }
      // If there exists a variadic method, it must have at least the same arity as the non-variadic method with the greatest arity
      auto last = methodFunctionMethod.second[methodFunctionMethod.second.size() - 1];
      bool lastVariadic = last.params().size() > last.fixedarity();
      if (lastVariadic && methodFunctionMethod.second.size() > 1) {
        auto variadicArity = last.fixedarity();
        auto nonvariadicArity = methodFunctionMethod.second[methodFunctionMethod.second.size() - 2].fixedarity();
        if (nonvariadicArity > variadicArity) {
          runtimeException(CodeGenerationException("Variadic method must have the highest arity in " + methodFunctionMethod.first, node));
          return TypedValue(ObjectTypeSet(nilType), dynamicNil());
        }
      }
    }
  }
  // Group class methods: generation
  Value *methodCount = ConstantInt::get(*TheContext, APInt(64, methodFunctionMethods.size(), false));
  std::vector<Type *> methodTypes {Type::getInt64Ty(*TheContext)};
  std::vector<Value *> methodNamesArgs {methodCount};
  std::vector<Value *> methodsArgs {methodCount};
  for (auto methodFunctionMethod: methodFunctionMethods) {
    methodTypes.push_back(ptrT);
    methodNamesArgs.push_back(dynamicKeyword(methodFunctionMethod.first.c_str()));
    
    Value *methodCount = ConstantInt::get(*TheContext, APInt(64, methodFunctionMethod.second.size(), false));
    uint64_t funId = getUniqueFunctionId();
    // It's fine to assign an unique ID and then not insert anything into TheProgramme->Functions?
    // No ,,grouping'' node, but methods won't be called in FnNode
    Value *uniqueId = ConstantInt::get(*TheContext, APInt(64, funId, false));
    uint64_t maxFixedArity = 0;
    auto last = methodFunctionMethod.second[methodFunctionMethod.second.size() - 1];
    bool lastVariadic = last.params().size() > last.fixedarity();
    if (lastVariadic) {
      if (methodFunctionMethod.second.size() > 1) {
        maxFixedArity = methodFunctionMethod.second[methodFunctionMethod.second.size() - 2].fixedarity();
      }
    } else {
      maxFixedArity = last.fixedarity();
    }
    Value *maxFixedArityValue = ConstantInt::get(*TheContext, APInt(64, maxFixedArity, false));
    Value *once = ConstantInt::get(*TheContext, APInt(8, 0, false)); // methods should never be once?
    
    vector<Type *> createFunTypes {Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext), Type::getInt8Ty(*TheContext)};
    vector<Value *> createFunArgs {methodCount, uniqueId, maxFixedArityValue, once};
    Value *fun = dynamicCreate(functionType, createFunTypes, createFunArgs);
    
    for (uint64_t i = 0; i < methodFunctionMethod.second.size(); ++i) {
      auto method = methodFunctionMethod.second[i];
      
      Value *position = ConstantInt::get(*TheContext, APInt(64, i, false));
      Value *index = ConstantInt::get(*TheContext, APInt(64, i, false)); // What is index? Position in method list before sorting?
      Value *fixedArity = ConstantInt::get(*TheContext, APInt(64, method.fixedarity(), false));
      Value *isVariadic = ConstantInt::get(*TheContext, APInt(8, method.params().size() > method.fixedarity(), false));
      Value *loopId = Builder->CreateGlobalStringPtr(StringRef(method.loopid().c_str()), "staticString");
      Value *closedOversCount = ConstantInt::get(*TheContext, APInt(64, 0, false)); // methods can't have closedovers
      vector <Type *> fillMethodTypes {ptrT, Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext), Type::getInt64Ty(*TheContext), Type::getInt8Ty(*TheContext), ptrT, Type::getInt64Ty(*TheContext)};
      vector <Value *> fillMethodArgs {fun, position, index, fixedArity, isVariadic, loopId, closedOversCount};
      callRuntimeFun("Function_fillMethod", Type::getVoidTy(*TheContext), fillMethodTypes, fillMethodArgs, true);
    }
    
    methodsArgs.push_back(fun);
  }
  Value *methodNames = callRuntimeFun("packPointerArgs", ptrT, methodTypes, methodNamesArgs, true);
  Value *methods = callRuntimeFun("packPointerArgs", ptrT, methodTypes, methodsArgs, true);
  
  // TODO: Interfaces
  Value *implementedInterfacesCount = ConstantInt::get(*TheContext, APInt(64, 0, false));
  Value *implementedInterfaces = Constant::getNullValue(ptrT);
  Value *implementedInterfacesMethods = Constant::getNullValue(ptrT);
  
  std::vector<std::pair<Type *, Value *>> argTypes {
    {ptrT, name},
    {ptrT, className},
    {Type::getInt64Ty(*TheContext), flags},
    
    {ptrT, superclassPtr},
    
    {Type::getInt64Ty(*TheContext), staticFieldCount},
    {ptrT, staticFieldNames},
    {ptrT, staticFields},
    
    {Type::getInt64Ty(*TheContext), staticMethodCount},
    {ptrT, staticMethodNames},
    {ptrT, staticMethods},
    
    {Type::getInt64Ty(*TheContext), fieldCount},
    {ptrT, fieldNames},
    
    {Type::getInt64Ty(*TheContext), methodCount},
    {ptrT, methodNames},
    {ptrT, methods},
    
    {Type::getInt64Ty(*TheContext), implementedInterfacesCount},
    {ptrT, implementedInterfaces},
    {ptrT, implementedInterfacesMethods}
  };
  
  Value *classValue = callRuntimeFun("Class_create", ptrT, argTypes);
  Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
  callRuntimeFun("registerClass", Type::getVoidTy(*TheContext), {ptrT, ptrT}, {statePtr, classValue});
  return TypedValue(ObjectTypeSet(nilType), dynamicNil());
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
