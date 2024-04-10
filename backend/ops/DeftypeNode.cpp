#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto name = dynamicString(subnode.name().c_str());
  auto classNameStr = subnode.classname();
  auto className = dynamicString(classNameStr.c_str());
  
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  std::vector<Type *> types {ptrT, ptrT, Type::getInt64Ty(*TheContext)};
  std::vector<Value *> args {name, className, ConstantInt::get(*TheContext, APInt(64, subnode.fields_size(), false))};
  for (auto field: subnode.fields()) {
    types.push_back(ptrT);
    args.push_back(dynamicString(field.subnode().binding().name().c_str()));
  }
  
  Value *classValue = callRuntimeFun("Class_create", ptrT, types, args, true);
  Value *selfPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) this, false)), ptrT);
  callRuntimeFun("registerClass", Type::getVoidTy(*TheContext), {ptrT, ptrT, ptrT}, {selfPtr, className, classValue});
  return TypedValue(ObjectTypeSet(nilType), dynamicNil());
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
