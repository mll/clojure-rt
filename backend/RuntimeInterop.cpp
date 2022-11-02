#include "codegen.h"  
#include "Numbers.h"

Value *CodeGenerator::callRuntimeFun(const string &fname, Type *retValType, const vector<Type *> &argTypes, const vector<Value *> &args) {
  Function *CalleeF = TheModule->getFunction(fname);
  
  if (!CalleeF) {
    FunctionType *FT = FunctionType::get(retValType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, fname, TheModule.get());
  }

  if (!CalleeF) throw InternalInconsistencyException(string("Unable to find ") + fname + " - do we link runtime?");
  
  return  Builder->CreateCall(CalleeF, args, string("call_") + fname);
} 

void CodeGenerator::runtimeException(const CodeGenerationException &runtimeException) {
  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(runtimeException.toString().c_str()), "dynamicString"));      
  callRuntimeFun("logException", Type::getVoidTy(*TheContext), argTypes, args);
}

Value *CodeGenerator::dynamicCreate(objectType type, const vector<Type *> &argTypes, const vector<Value *> &args) {
  string fname = "";
  switch(type) {
    case integerType:
      fname = "Integer_create";
      break;
    case doubleType:
      fname = "Double_create";
      break;
    case booleanType:
      fname = "Boolean_create";
      break;
    case stringType:
      fname = "String_create_copy";
      break;
    case persistentListType:
      fname = "PersistentList_create";
      break;
    case persistentVectorType:
      fname = "PersistentVector_create";
      break;
    case persistentVectorNodeType:
      throw InternalInconsistencyException("We never allow creation of subtypes here, only runtime can do it");
    case nilType:
      fname = "Nil_create";
      break;
    case symbolType:
      fname = "Symbol_create";
      break;
  }

  return callRuntimeFun(fname, dynamicBoxedType(type), argTypes, args);
}

Value * CodeGenerator::dynamicNil() {
  return dynamicCreate(nilType, vector<Type *>(), vector<Value *>());
}

Value * CodeGenerator::dynamicString(const char *str) {
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(str), "dynamicString"));
  return dynamicCreate(stringType, types, args);
}


Value * CodeGenerator::dynamicSymbol(const char *ns, const char *name) {
  auto nss = dynamicString(ns);
  auto names = dynamicString(name);
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());


  args.push_back(names);
  args.push_back(nss);
  return dynamicCreate(symbolType, types, args);
}



Type *CodeGenerator::dynamicUnboxedType(objectType type) {
  switch(type) {
    case integerType:
      return Type::getInt64Ty(*TheContext);
    case doubleType:
      return Type::getDoubleTy(*TheContext);
    case booleanType:
      return Type::getInt1Ty(*TheContext);
    case stringType:
    case persistentListType:
    case persistentVectorType:
    case persistentVectorNodeType:
    case nilType:
    case symbolType:
      return Type::getInt8Ty(*TheContext)->getPointerTo();
  }
}

Type *CodeGenerator::dynamicBoxedType(objectType type) {
  return Type::getInt8Ty(*TheContext)->getPointerTo();
}

Value *CodeGenerator::box(const TypedValue &value) {
  if (!value.first.isDetermined()) return value.second;

  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(dynamicUnboxedType(value.first.determinedType()));
  
  switch(value.first.determinedType()) {
  case integerType:      
  case doubleType:
    args.push_back(value.second);
    break;
  case booleanType:
    args.push_back(Builder->CreateIntCast(value.second, Type::getInt8Ty(*TheContext), false));
    break;
  case stringType:
  case persistentListType:
  case persistentVectorType:
  case persistentVectorNodeType:
  case nilType:
  case symbolType:
    return value.second;
  }
  return dynamicCreate(value.first.determinedType(), argTypes, args);
}

Value *CodeGenerator::dynamicCond(Value *cond) {
  /* We assume value is always of boxed pointer type here */
  /* TODO: this could be inlined for speed, for now we call into runtime (slower) */
  vector<Type *> argTypes = { Type::getInt8Ty(*TheContext)->getPointerTo() };
  vector<Value *> args = { cond };
  Value *int8bool = callRuntimeFun("logicalValue", Type::getInt8Ty(*TheContext), argTypes, args); 
  return Builder->CreateIntCast(int8bool, dynamicUnboxedType(booleanType), false);
}

/* LOAD-STORE examples */

//   Type *objectTypeType = Type::getInt8Ty(*TheContext)->getPointerTo();    
  
//   /* For now we use a call to 'super' from the runtime. In the future we will replace it with our own fast inlined computation */
  
// // cast (void *) arg back to original arg type
//   llvm::Value *argVoidPtr = asyncFun->args().begin();
//   llvm::Value *argStructPtr = 
//       builder->CreatePointerCast(argVoidPtr, argStructTy->getPointerTo());

//   // allocate function args on the stack
//   for (int i = 0; i < asyncExpr.freeVars.size(); i++) {
//     llvm::Value *freeVarVal = builder->CreateLoad(
//         builder->CreateStructGEP(argStructTy, argStructPtr, i));
//     varEnv[asyncExpr.freeVars[i]] = builder->CreateAlloca(
//         freeVarVal->getType(), nullptr, llvm::Twine(asyncExpr.freeVars[i]));
//     builder->CreateStore(freeVarVal, varEnv[asyncExpr.freeVars[i]]);
  

