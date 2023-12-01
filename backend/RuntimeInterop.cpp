#include "codegen.h"  
#include <stdio.h>

using namespace std;
using namespace llvm;

extern "C" {
  #include "runtime/String.h"
  #include "runtime/Keyword.h"
} 


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
  
  if(retValType == Type::getVoidTy(*TheContext)) return Builder->CreateCall(CalleeF, args);
  return Builder->CreateCall(CalleeF, args, string("call_") + fname);
} 

void CodeGenerator::runtimeException(const CodeGenerationException &runtimeException) {
  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(runtimeException.toString().c_str()), "dynamicString"));      
  callRuntimeFun("logException", Type::getVoidTy(*TheContext), argTypes, args);
}

void CodeGenerator::logString(const string &s) {
  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(Builder->CreateGlobalStringPtr(StringRef(s.c_str()), "dynamicString"));      
  callRuntimeFun("logText", Type::getVoidTy(*TheContext), argTypes, args);
}

void CodeGenerator::logDebugBoxed(llvm::Value *v) {
  logType(getRuntimeObjectType(v));
  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(v);      
  Value *s = callRuntimeFun("toString", Type::getInt8Ty(*TheContext)->getPointerTo(), argTypes, args);
  args.clear();
  args.push_back(s);      
  Value *ss = callRuntimeFun("String_c_str", Type::getInt8Ty(*TheContext)->getPointerTo(), argTypes, args);
  args.clear();
  args.push_back(ss);      
  callRuntimeFun("logText", Type::getVoidTy(*TheContext), argTypes, args);
}


void CodeGenerator::logType(Value *v) {
  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(Type::getInt32Ty(*TheContext));
  args.push_back(v);      
  callRuntimeFun("logType", Type::getVoidTy(*TheContext), argTypes, args);
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
  case persistentArrayMapType:
    fname = "PersistentArrayMap_createMany";
    isVariadic = true;      
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

Value *CodeGenerator::dynamicArrayMap(const vector<TypedValue> &args) {
  vector<Type *> types;
  vector<Value *> argsV;
  /* arg count must be smaller than 33 */
  types.push_back(Type::getInt64Ty(*TheContext));
  argsV.push_back(ConstantInt::get(*TheContext, APInt(64, args.size() / 2)));
  for(auto arg: args) {
    argsV.push_back(box(arg).second);
  }

  auto retVal = dynamicCreate(persistentArrayMapType, types, argsV);
  /* TODO: RefCount - This remains to be discovered - depending on the refcount strategy it might be needed or not */
//  for(int i=0; i<args.size(); i++ ) dynamicRelease(argsV[i+1]); 
  return retVal;
}



/********************** TYPES *********************************/

/* 
   struct Object {
     objectType type;
     volatile atomic_uint_fast64_t atomicRefCount;
   };
*/

StructType *CodeGenerator::runtimeObjectType() {
  StructType *retVal = StructType::getTypeByName(*TheContext,"Object");
  if(retVal) return retVal;
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
  StructType *retVal = StructType::getTypeByName(*TheContext,"Function");
  if(retVal) return retVal;

   return StructType::create(*TheContext, {
       /* uniqueId */ dynamicUnboxedType(integerType),
       /* methodCount */ dynamicUnboxedType(integerType),
       /* maxArity */ dynamicUnboxedType(integerType),
       /* TODO : Methods */
     }, "Function");
}

/* struct Integer {
   int64_t value;
}; */

StructType *CodeGenerator::runtimeIntegerType() {
  StructType *retVal = StructType::getTypeByName(*TheContext,"Integer");
  if(retVal) return retVal;

   return StructType::create(*TheContext, {
       /* value */ Type::getInt64Ty(*TheContext),
     }, "Integer");
}

/* struct Double {
  double value;
}; */

StructType *CodeGenerator::runtimeDoubleType() {
  StructType *retVal = StructType::getTypeByName(*TheContext,"Double");
  if(retVal) return retVal;

   return StructType::create(*TheContext, {
       /* value */ Type::getDoubleTy(*TheContext),
     }, "Double");
}

/* struct Boolean {
  unsigned char value;
}; */

StructType *CodeGenerator::runtimeBooleanType() {
  StructType *retVal = StructType::getTypeByName(*TheContext,"Boolean");
  if(retVal) return retVal;

   return StructType::create(*TheContext, {
       /* value */ Type::getInt8Ty(*TheContext),
     }, "Boolean");
}


void CodeGenerator::dynamicRetain(Value *objectPtr) {
  vector<Type *> t;
  vector<Value * > v;
  //callRuntimeFun("printReferenceCounts", Type::getVoidTy(*TheContext), t, v, false);
  t.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  v.push_back(objectPtr);
  callRuntimeFun("retain", Type::getVoidTy(*TheContext),t, v);
 // t.clear();
//  v.clear();
  // Value *objPtr = dynamicSuper(objectPtr);
  // Value *gepPtr = Builder->CreateStructGEP(runtimeObjectType(), objPtr, 1, "get_type");  
  // Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Add, gepPtr, ConstantInt::get(*TheContext, APInt(64, 1)), MaybeAlign(), AtomicOrdering::Monotonic);
  //callRuntimeFun("printReferenceCounts", Type::getVoidTy(*TheContext), t, v, false);
}


Value *CodeGenerator::dynamicSuper(Value *objectPtr) {
 /* A trick - objSize will have sizeof(Object) */
  vector<Type *> t;
  vector<Value * > v;
  //callRuntimeFun("printReferenceCounts", Type::getVoidTy(*TheContext), t, v, false);
  t.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  v.push_back(objectPtr);
  return callRuntimeFun("super", Type::getInt8Ty(*TheContext)->getPointerTo(),t, v);
  
  // Value *objDummyPtr = Builder->CreateConstGEP1_64(runtimeObjectType(), Constant::getNullValue(runtimeObjectType()->getPointerTo()), 1, "Object_size");
  // Value *objSize =  Builder->CreatePointerCast(objDummyPtr, Type::getInt64Ty(*TheContext));
  // Value *funcPtr = Builder->CreateBitOrPointerCast(objectPtr, Type::getInt8Ty(*TheContext)->getPointerTo(), "void_to_unboxed");
  // Value *funcPtrInt = Builder->CreatePtrToInt(funcPtr,  Type::getInt64Ty(*TheContext), "ptr_to_int");
  // Value *objPtrInt = Builder->CreateSub(funcPtrInt, objSize, "sub_size");
  // Value *objPtr = Builder->CreateIntToPtr(objPtrInt, runtimeObjectType()->getPointerTo() , "sub_size");  
  // return objPtr;
}

Value *CodeGenerator::getRuntimeObjectType(Value *objectPtr) {
  vector<Type *> t;
  vector<Value * > v;
  //callRuntimeFun("printReferenceCounts", Type::getVoidTy(*TheContext), t, v, false);
  t.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  v.push_back(objectPtr);
  return callRuntimeFun("getType", Type::getInt32Ty(*TheContext),t, v);

  // Value *objPtr = dynamicSuper(objectPtr);
  // Value *gepPtr = Builder->CreateStructGEP(runtimeObjectType(), objPtr, 0, "get_type");
  // Value *retVal = Builder->CreateLoad(Type::getInt32Ty(*TheContext), gepPtr, "load_type");    
  // return retVal;
}

TypedValue CodeGenerator::dynamicIsReusable(Value *what) {
  assert(what->getType() == Type::getInt8Ty(*TheContext)->getPointerTo() && "Not a pointer!");
  vector<Type *> t;
  vector<Value * > v;
  //callRuntimeFun("printReferenceCounts", Type::getVoidTy(*TheContext), t, v, false);
  t.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  v.push_back(what);
  return TypedValue(ObjectTypeSet(booleanType), Builder->CreateIntCast(callRuntimeFun("isReusable", Type::getInt8Ty(*TheContext),t, v), dynamicUnboxedType(booleanType), false));
 
  // Value *objPtr = dynamicSuper(what);
  // Value *gepPtr = Builder->CreateStructGEP(runtimeObjectType(), objPtr, 1, "get_type");
  // LoadInst *load = Builder->CreateLoad(Type::getInt64Ty(*TheContext), gepPtr, "load_type");    
  // load->setAtomic(AtomicOrdering::Monotonic);
  // Value *retVal = Builder->CreateICmpEQ(load, ConstantInt::get(*TheContext, APInt(64, 1, false)), "cmp_jj");
  // return TypedValue(ObjectTypeSet(booleanType), retVal);
}

Value * CodeGenerator::dynamicString(const char *str) {
  String * retVal = String_createDynamicStr((char *)str);
  /* TODO: uniquing? */
  Value *strPointer = Builder->CreateBitOrPointerCast(ConstantInt::get(*TheContext, APInt(64, (int64_t) retVal, false)), Type::getInt8Ty(*TheContext)->getPointerTo(), "void_to_unboxed");
  return strPointer;
}

TypedValue CodeGenerator::dynamicRelease(Value *what, bool isAutorelease = false) {
  vector<Type *> types;
  vector<Value *> args;
  types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  args.push_back(what);    
  callRuntimeFun(isAutorelease ? "autorelease" : "release", Type::getVoidTy(*TheContext), types, args); 
  return TypedValue(ObjectTypeSet(booleanType), ConstantInt::get(*TheContext, APInt(1, 0, false)));
  
  // Function *parentFunction = Builder->GetInsertBlock()->getParent();
  // Value *object = dynamicSuper(what);

  // Value *oldValue = Builder->CreateAtomicRMW(AtomicRMWInst::BinOp::Sub, object, ConstantInt::get(*TheContext, APInt(64, 1, false)), MaybeAlign(), AtomicOrdering::Monotonic);
  // Value *condValue = Builder->CreateICmpEQ(oldValue, ConstantInt::get(*TheContext, APInt(64, 1, false)), "cmp_jj");
  
  // BasicBlock *destroyBB = llvm::BasicBlock::Create(*TheContext, "destroy", parentFunction);  
  // BasicBlock *ignoreBB = llvm::BasicBlock::Create(*TheContext, "ignore");
  // BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "release_cont");
  
  // Builder->CreateCondBr(condValue, destroyBB, ignoreBB);

  // Builder->SetInsertPoint(destroyBB);
  // vector<Type *> types;
  // vector<Value *> args;
  // types.push_back(Type::getInt8Ty(*TheContext)->getPointerTo());
  // args.push_back(object);

  // types.push_back(Type::getInt8Ty(*TheContext));
  // args.push_back(ConstantInt::get(*TheContext, APInt(8, 1, false)));
    
  // callRuntimeFun("Object_destroy", Type::getVoidTy(*TheContext), types, args); 
  // Builder->CreateBr(mergeBB);

  // parentFunction->getBasicBlockList().push_back(ignoreBB);
  // Builder->SetInsertPoint(ignoreBB);
  // Builder->CreateBr(mergeBB);
  
  // parentFunction->getBasicBlockList().push_back(mergeBB);
  // Builder->SetInsertPoint(mergeBB);
  
  // llvm::PHINode *phiNode = Builder->CreatePHI(Type::getInt1Ty(*TheContext), 2, "release_phi");
  // phiNode->addIncoming(ConstantInt::get(*TheContext, APInt(1, 1, false)), destroyBB);
  // phiNode->addIncoming(ConstantInt::get(*TheContext, APInt(1, 0, false)), ignoreBB);
  // return TypedValue(ObjectTypeSet(booleanType), phiNode);
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
  String *names = String_createDynamicStr((char *)name);
  
  Keyword * retVal = Keyword_create(names);
  Value *ptrKeyword =  Builder->CreateBitOrPointerCast(ConstantInt::get(*TheContext, APInt(64, (int64_t) retVal, false)), Type::getInt8Ty(*TheContext)->getPointerTo(), "void_to_unboxed");
  return ptrKeyword;
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
    case persistentArrayMapType:
    case functionType:
    case concurrentHashMapType:
      return Type::getInt8Ty(*TheContext)->getPointerTo();
  }
}

Value *CodeGenerator::dynamicZero(const ObjectTypeSet &type) {
  if(!type.isScalar()) return ConstantPointerNull::get(dynamicBoxedType());
  switch(type.determinedType()) {
    case integerType:
      return ConstantInt::get(*TheContext, APInt(64, 0, false));
    case booleanType:
      return ConstantInt::get(*TheContext, APInt(1, 0, false));
    case doubleType:
      return ConstantFP::get(*TheContext, APFloat(0.0));
    default:
      break;
  }
  return ConstantPointerNull::get(dynamicBoxedType());
}

Type *CodeGenerator::dynamicBoxedType(objectType type) {
  return Type::getInt8Ty(*TheContext)->getPointerTo();
}

PointerType *CodeGenerator::dynamicBoxedType() {
  return Type::getInt8Ty(*TheContext)->getPointerTo();
}

Type *CodeGenerator::dynamicType(const ObjectTypeSet &type) {
  if (type.isScalar()) return dynamicUnboxedType(type.determinedType());
  return dynamicBoxedType();
}

pair<BasicBlock *, Value *> CodeGenerator::dynamicUnbox(const Node &node, const TypedValue &value, objectType forcedType) {
  const ObjectTypeSet &dynType = value.first;
  if(dynType.isScalar() || dynType.isDetermined()) {
    if(forcedType != dynType.determinedType()) throw CodeGenerationException(string("Unable to unbox: ") + dynType.toString() + " to " + to_string(forcedType), node);
    return {Builder->GetInsertBlock(), unbox(value).second};
  }

  Type *unboxType = nullptr;
  StructType *stype = nullptr;
  switch(forcedType) {
  case integerType:
    stype = runtimeIntegerType();
    unboxType = Type::getInt64Ty(*TheContext);
    break;
  case doubleType:
    stype = runtimeDoubleType();
    unboxType = Type::getDoubleTy(*TheContext);
    break;
  case booleanType:
    stype = runtimeBooleanType();
    unboxType = Type::getInt8Ty(*TheContext);
    break;
  default:
    unboxType = Type::getInt8Ty(*TheContext)->getPointerTo();
    break;
  }

  Value *type = getRuntimeObjectType(value.second);
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  
  BasicBlock *wrongBB = llvm::BasicBlock::Create(*TheContext, "failed_dynamic_cast", parentFunction);
  BasicBlock *mergeBB = llvm::BasicBlock::Create(*TheContext, "merge_dynamic_cast", parentFunction);    

  Value *cond = Builder->CreateICmpEQ(type, ConstantInt::get(*TheContext, APInt(32, forcedType, false)) , "cmp_type");
  Builder->CreateCondBr(cond, mergeBB, wrongBB);
      
  Builder->SetInsertPoint(wrongBB);
  runtimeException(CodeGenerationException("Unable to dynamically unbox " + to_string(forcedType), node));    
  Builder->CreateBr(mergeBB);
  
  Builder->SetInsertPoint(mergeBB);
  
  Value *loaded = nullptr;
  if(unboxType == Type::getInt8Ty(*TheContext)->getPointerTo()) loaded = value.second;
  else {
    Value *ptr = Builder->CreateBitOrPointerCast(value.second, stype->getPointerTo(), "void_to_struct");
    Value *tPtr = Builder->CreateStructGEP(stype, ptr, 0, "struct_gep");
    
    loaded = Builder->CreateLoad(unboxType, tPtr, "load_var");
    if(forcedType ==booleanType) loaded = Builder->CreateIntCast(loaded, dynamicUnboxedType(booleanType), false);
  }
  
  return pair<BasicBlock *, Value *>(mergeBB, loaded);
}

TypedValue CodeGenerator::unbox(const TypedValue &value) {
  const ObjectTypeSet &dynType = value.first;
  if(dynType.isScalar()) return value;
  if(!dynType.isDetermined()) throw InternalInconsistencyException(string("Unable to statically unbox type: ") + dynType.toString());
  ObjectTypeSet t = dynType.unboxed();

  StructType *stype = nullptr;
  Type *type = nullptr;
  
  switch(value.first.determinedType()) {
  case integerType:
    stype = runtimeIntegerType();
    type = Type::getInt64Ty(*TheContext);
    break;
  case doubleType:
    stype = runtimeDoubleType();
    type = Type::getDoubleTy(*TheContext);
    break;
  case booleanType:
    stype = runtimeBooleanType();
    type = Type::getInt8Ty(*TheContext);
    break;
  default:
    return TypedValue(t, value.second);
  }

  Value *ptr = Builder->CreateBitOrPointerCast(value.second, stype->getPointerTo(), "void_to_struct");
  Value *tPtr = Builder->CreateStructGEP(stype, ptr, 0, "struct_gep");

  Value *loaded = Builder->CreateLoad(type, tPtr, "load_var");
  if(dynType.isBoxedType(booleanType)) loaded = Builder->CreateIntCast(loaded, dynamicUnboxedType(booleanType), false);
  // TODO: memory management
//  dynamicRelease(value.second);
  return TypedValue(t, loaded);
}
                                        
TypedValue CodeGenerator::box(const TypedValue &value) {
  const ObjectTypeSet &type = value.first;
  if (!type.isScalar()) return value;

  vector<Type *> argTypes;
  vector<Value *> args;
  argTypes.push_back(value.first.determinedType() == booleanType ? Type::getInt8Ty(*TheContext) : dynamicUnboxedType(value.first.determinedType()));
  ObjectTypeSet retType = type.boxed();

  switch(type.determinedType()) {
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
  case persistentArrayMapType:
  case functionType:
    return TypedValue(retType, value.second);
  }
  return TypedValue(retType, dynamicCreate(type.determinedType(), argTypes, args));
}

Value *CodeGenerator::dynamicCond(Value *cond) {
  /* We assume value is always of boxed pointer type here */
  /* TODO: this could be inlined for speed, for now we call into runtime (slower) */
  vector<Type *> argTypes = { Type::getInt8Ty(*TheContext)->getPointerTo() };
  vector<Value *> args = { cond };
  Value *int8bool = callRuntimeFun("logicalValue", Type::getInt8Ty(*TheContext), argTypes, args); 
  return Builder->CreateIntCast(int8bool, dynamicUnboxedType(booleanType), false);
}

/* Applies memory operations (retains/releases) according to the guidance generated by the frontend.
   In the future, ( TODO ) a single retain/release call could be made with a counter (if appropriate runtime modification is done) */

void CodeGenerator::dynamicMemoryGuidance(const MemoryManagementGuidance &guidance) {
  auto name = guidance.variablename();
  auto change = guidance.requiredrefcountchange();
  auto found = StaticVars.find(name);
  
  if(found != StaticVars.end()) {      
    auto type = found->second.first;
    if(!type.isDynamic()) return;
    
    Type *t = dynamicType(found->second.first);
    
    LoadInst * load = Builder->CreateLoad(t, found->second.second, "load_var");
    load->setAtomic(AtomicOrdering::Monotonic);
    while(change != 0) {
      if(change > 0) { dynamicRetain(load); change--; }
      else { dynamicRelease(load, false); change++; }
    }
    return;
  }
  
  for(int j=VariableBindingStack.size() - 1; j >= 0; j--) {
    auto stack = VariableBindingStack[j];
    auto found = stack.find(name);
    if(found!=stack.end()) {
      auto typedValue = found->second;
      if(!typedValue.first.isDynamic()) break;
      while(change != 0) {
        if(change > 0) { dynamicRetain(typedValue.second); change--; }
        else { dynamicRelease(typedValue.second, false); change++; }
      }
      break;
    }
  }  
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
  

