#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto className = subnode.class_().subnode().const_().val();
  auto stringPtr = dynamicString(className.c_str());
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  auto selfPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) this, false)), ptrT);
  Value *classValue = callRuntimeFun("getClass", ptrT, {ptrT, ptrT}, {selfPtr, stringPtr});
  Function *parentFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock *classFound = BasicBlock::Create(*TheContext, "class_found", parentFunction);
  BasicBlock *classMissing = BasicBlock::Create(*TheContext, "class_missing", parentFunction);
  Value *classNotFoundValue = Builder->CreateICmpEQ(classValue, Constant::getNullValue(ptrT));
  Builder->CreateCondBr(classNotFoundValue, classMissing, classFound);
  
  Builder->SetInsertPoint(classMissing);
  runtimeException(CodeGenerationException("Class " + className + " not found", node));
  Builder->CreateUnreachable();
  
  Builder->SetInsertPoint(classFound);
  BasicBlock *arityFound = BasicBlock::Create(*TheContext, "arity_found", parentFunction);
  BasicBlock *arityMissing = BasicBlock::Create(*TheContext, "arity_missing", parentFunction);
  Value *calledArity = ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, subnode.args_size(), false));
  Value *expectedArityPtr = Builder->CreateStructGEP(runtimeClassType(), classValue, 2, "expected_arity_ptr");
  Value *expectedArity = Builder->CreateLoad(Type::getInt64Ty(*TheContext), expectedArityPtr, "expected_arity");
  Value *correctArity = Builder->CreateICmpEQ(expectedArity, calledArity);
  Builder->CreateCondBr(correctArity, arityFound, arityMissing);
  
  Builder->SetInsertPoint(arityMissing);
  runtimeException(CodeGenerationException("Class " + className + " does not have a constructor of arity " + to_string(subnode.args_size()), node));
  Builder->CreateUnreachable();
  
  Builder->SetInsertPoint(arityFound);
  std::vector<Type *> types {ptrT, Type::getInt64Ty(*TheContext)};
  std::vector<Value *> args {classValue, calledArity};
  for (auto arg: subnode.args()) {
    types.push_back(ptrT);
    args.push_back(box(codegen(arg, ObjectTypeSet::all())).second);
  }
  
  Value *deftypeValue = callRuntimeFun("Deftype_create", ptrT, types, args, true);
  return TypedValue(ObjectTypeSet(deftypeType), deftypeValue);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const NewNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(deftypeType);
}
