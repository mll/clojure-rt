#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto name = dynamicString(subnode.name().c_str());
  dynamicRetain(name);
  auto classNameStr = subnode.classname(); // contains "class " prefix
  auto className = dynamicString(classNameStr.c_str());
  dynamicRetain(className);
  
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  std::vector<Type *> types {ptrT, ptrT, Type::getInt64Ty(*TheContext)};
  std::vector<Value *> args {name, className, ConstantInt::get(*TheContext, APInt(64, subnode.fields_size(), false))};
  for (auto field: subnode.fields()) {
    types.push_back(ptrT);
    auto fieldName = dynamicString(field.subnode().binding().name().c_str());
    dynamicRetain(fieldName);
    args.push_back(fieldName);
  }
  
  Value *classValue = callRuntimeFun("Class_create", ptrT, types, args, true);
  Value *thisPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) this, false)), ptrT);
  callRuntimeFun("registerClass", Type::getVoidTy(*TheContext), {ptrT, ptrT}, {thisPtr, classValue});
  return TypedValue(ObjectTypeSet(nilType), dynamicNil());
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
