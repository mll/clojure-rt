#include "codegen.h"  
#include <stdio.h>

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::callRuntimeFun(const string &fname, const ObjectTypeSet &retVal, const vector<TypedValue> &args) {
  Function *CalleeF = TheModule->getFunction(fname);
  
  vector<Type *> argTypes;
  vector<Value *> argVals;
  for(auto arg: args) { 
    argTypes.push_back(dynamicType(arg.first));
    argVals.push_back(arg.second);
  }
  auto retValType = dynamicType(retVal);
  

  if (!CalleeF) {
    FunctionType *FT = FunctionType::get(retValType, argTypes, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, fname, TheModule.get());
  }

  if (!CalleeF) throw InternalInconsistencyException(string("Unable to find ") + fname + " - do we link runtime?");
  
  return  TypedValue(retVal, Builder->CreateCall(CalleeF, argVals, string("call_") + fname));
} 



Value *CodeGenerator::callRuntimeFun(const string &fname, Type *retValType, const vector<Type *> &argTypes, const vector<Value *> &args, bool isVariadic) {
  Function *CalleeF = TheModule->getFunction(fname);
  
  if (!CalleeF) {
    FunctionType *FT = FunctionType::get(retValType, argTypes, isVariadic);
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
  bool isVariadic = false;
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
      fname = "String_createStaticOptimised";
      break;
    case persistentListType:
      fname = "PersistentList_create";
      break;
    case persistentVectorType:
      fname = "PersistentVector_createMany";
      isVariadic = true;      
      break;
    case persistentVectorNodeType:
      throw InternalInconsistencyException("We never allow creation of subtypes here, only runtime can do it");
    case nilType:
      fname = "Nil_create";
      break;
    case symbolType:
      fname = "Symbol_create";
      break;
    case keywordType:
      fname = "Keyword_create";
      break;
    case concurrentHashMapType:
      fname = "ConcurrentHashMap_create";
      break;
    case functionType:
      fname = "Function_create";
      break;
  }

  return callRuntimeFun(fname, dynamicBoxedType(type), argTypes, args, isVariadic);
}

Value * CodeGenerator::dynamicNil() {
  return dynamicCreate(nilType, vector<Type *>(), vector<Value *>());
}

uint64_t CodeGenerator::avalanche_64(uint64_t h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}


uint64_t CodeGenerator::computeHash(const char *str) {
    uint64_t h = 5381;
    uint64_t c;

    while ((c = *str++)) h += avalanche_64(c);
    return h;
}

Value *CodeGenerator::dynamicVector(const vector<TypedValue> &args) {
  vector<Type *> types;
  vector<Value *> argsV;
  types.push_back(Type::getInt64Ty(*TheContext));
  argsV.push_back(ConstantInt::get(*TheContext, APInt(64, args.size())));
  for(auto arg: args) {
    argsV.push_back(box(arg).second);
  }

  auto retVal = dynamicCreate(persistentVectorType, types, argsV);
  /* TODO: RefCount - This remains to be discovered - depending on the refcount strategy it might be needed or not */
//  for(int i=0; i<args.size(); i++ ) dynamicRelease(argsV[i+1]); 
  return retVal;
}


/* 
   struct Object {
     objectType type;
     volatile atomic_uint_fast64_t atomicRefCount;
   };
*/

StructType *CodeGenerator::runtimeObjectType() {
   return StructType::create(*TheContext, {
       /* type */ Type::getInt32Ty(*TheContext),
       /* atomicRefCount */ dynamicUnboxedType(integerType) }, "Object");
}

/* struct Function {
   uint64_t uniqueId;
   uint64_t methodCount;
   uint64_t maxArity;
   FunctionMethod methods[];
}; */

StructType *CodeGenerator::runtimeFunctionType() {
   return StructType::create(*TheContext, {
       /* uniqueId */ dynamicUnboxedType(integerType),
       /* methodCount */ dynamicUnboxedType(integerType),
       /* maxArity */ dynamicUnboxedType(integerType),
       /* TODO : Methods */
     }, "Function");
}


void CodeGenerator::dynamicRetain(Value *objectPtr) {
  /* A trick - objSize will have sizeof(Object) */
  Value *objDummyPtr = Builder->CreateConstGEP1_64(runtimeObjectType()->getPointerTo(), Constant::getNullValue(runtimeObjectType()->getPointerTo()), 1, "Object_size");
  Value *objSize =  Builder->CreatePointerCast(objDummyPtr, Type::getInt64Ty(*TheContext));
  
  Value *funcPtr = Builder->CreateBitOrPointerCast(objectPtr, Type::getInt8Ty(*TheContext)->getPointerTo(), "void_to_unboxed");
  Value *funcPtrInt = Builder->CreatePtrToInt(funcPtr,  Type::getInt64Ty(*TheContext), "ptr_to_int");
  Value *objPtrInt = Builder->CreateSub(funcPtrInt, objSize, "sub_size");
  Value *objPtr = Builder->CreateIntToPtr(objPtrInt, runtimeObjectType()->getPointerTo() , "sub_size");
  Value *gepPtr = Builder->CreateStructGEP(runtimeObjectType(), objPtr, 1, "get_type");
  
  Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Add, gepPtr, ConstantInt::get(*TheContext, APInt(64, 1)), MaybeAlign(), AtomicOrdering::Monotonic);
}

Value *CodeGenerator::getRuntimeObjectType(Value *objectPtr) {
  /* A trick - objSize will have sizeof(Object) */
  Value *objDummyPtr = Builder->CreateConstGEP1_64(runtimeObjectType(), Constant::getNullValue(runtimeObjectType()->getPointerTo()), 1, "Object_size");
  Value *objSize =  Builder->CreatePointerCast(objDummyPtr, Type::getInt64Ty(*TheContext));
  Value *funcPtr = Builder->CreateBitOrPointerCast(objectPtr, Type::getInt8Ty(*TheContext)->getPointerTo(), "void_to_unboxed");
  Value *funcPtrInt = Builder->CreatePtrToInt(funcPtr,  Type::getInt64Ty(*TheContext), "ptr_to_int");
  Value *objPtrInt = Builder->CreateSub(funcPtrInt, objSize, "sub_size");
  Value *objPtr = Builder->CreateIntToPtr(objPtrInt, runtimeObjectType()->getPointerTo() , "sub_size");
  Value *gepPtr = Builder->CreateStructGEP(runtimeObjectType(), objPtr, 0, "get_type");
  Value *retVal = Builder->CreateLoad(Type::getInt8Ty(*TheContext), gepPtr, "load_type");    
  return retVal;
}


Value * CodeGenerator::dynamicString(const char *str) {
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  types.push_back(Type::getInt64Ty(*TheContext));
  types.push_back(Type::getInt64Ty(*TheContext));
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(str), "staticString"));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, strlen(str))));
  args.push_back(ConstantInt::get(*TheContext, APInt(64, computeHash(str), false)));
  
  return dynamicCreate(stringType, types, args);
}

Value * CodeGenerator::dynamicRelease(Value *what, bool isAutorelease = false) {
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(what);

  return callRuntimeFun(isAutorelease ? "autorelease" : "release", isAutorelease ? Type::getVoidTy(*TheContext) : Type::getInt8Ty(*TheContext), types, args);
}



Value * CodeGenerator::dynamicSymbol(const char *name) {
  auto names = dynamicString(name);
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());

  args.push_back(names);
  return dynamicCreate(symbolType, types, args);
}

Value * CodeGenerator::dynamicKeyword(const char *name) {
  auto names = dynamicString(name);
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());

  args.push_back(names);
  return dynamicCreate(keywordType, types, args);
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
    case keywordType:
    case functionType:
    case concurrentHashMapType:
      return Type::getInt8Ty(*TheContext)->getPointerTo();
  }
}

Type *CodeGenerator::dynamicBoxedType(objectType type) {
  return Type::getInt8Ty(*TheContext)->getPointerTo();
}

Type *CodeGenerator::dynamicBoxedType() {
  return Type::getInt8Ty(*TheContext)->getPointerTo();
}

Type *CodeGenerator::dynamicType(const ObjectTypeSet &type) {
  if (type.isDetermined()) return dynamicUnboxedType(type.determinedType());
  return dynamicBoxedType();
}

TypedValue CodeGenerator::unbox(const TypedValue &value) {
  if(!value.first.isBoxed || !value.first.isDetermined()) return value;
  Type *type = nullptr;
  ObjectTypeSet t = value.first;
  t.isBoxed = false;
  
  switch(value.first.determinedType()) {
  case integerType:
    type = Type::getInt64Ty(*TheContext);
    break;
  case doubleType:
    type = Type::getDoubleTy(*TheContext);
    break;
  case booleanType:
    type = Type::getInt8Ty(*TheContext);
    break;
  default:
    return TypedValue(t, value.second);
  }

  Value *loaded = Builder->CreateLoad(type, Builder->CreateBitOrPointerCast(value.second, type->getPointerTo(), "void_to_unboxed"), "load_var");
  if(value.first.isType(booleanType)) loaded = Builder->CreateIntCast(loaded, dynamicUnboxedType(booleanType), false);
  dynamicRelease(value.second);
  return TypedValue(t, loaded);
}
                                        
TypedValue CodeGenerator::box(const TypedValue &value) {
  if (!value.first.isDetermined() || value.first.isBoxed) return value;

  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(dynamicUnboxedType(value.first.determinedType()));
  ObjectTypeSet type = value.first;
  type.isBoxed = true;

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
  case keywordType:
  case concurrentHashMapType:
  case functionType:
    return TypedValue(type, value.second);
  }
  return TypedValue(type, dynamicCreate(value.first.determinedType(), argTypes, args));
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
  

