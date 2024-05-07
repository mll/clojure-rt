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
  
  std::vector<Type *> fieldTypes {Type::getInt64Ty(*TheContext)};
  std::vector<Value *> fieldArgs {ConstantInt::get(*TheContext, APInt(64, subnode.fields_size(), false))};
  for (auto field: subnode.fields()) {
    fieldTypes.push_back(ptrT);
    fieldArgs.push_back(dynamicKeyword(field.subnode().binding().name().c_str()));
  }
  Value *packedFields = callRuntimeFun("packPointerArgs", ptrT, fieldTypes, fieldArgs, true);
  
  std::vector<std::pair<Type *, Value *>> argTypes {
    {ptrT, name},
    {ptrT, className},
    {Type::getInt64Ty(*TheContext), ConstantInt::get(*TheContext, APInt(64, 0, false))}, // no flags for now
    
    {Type::getInt64Ty(*TheContext), ConstantInt::get(*TheContext, APInt(64, 0, false))}, // no static fields
    {ptrT, Constant::getNullValue(ptrT)},
    {ptrT, Constant::getNullValue(ptrT)},
    
    {Type::getInt64Ty(*TheContext), ConstantInt::get(*TheContext, APInt(64, 0, false))}, // no static methods
    {ptrT, Constant::getNullValue(ptrT)},
    {ptrT, Constant::getNullValue(ptrT)},
    
    {Type::getInt64Ty(*TheContext), ConstantInt::get(*TheContext, APInt(64, subnode.fields_size(), false))},
    {ptrT, packedFields},
    
    {Type::getInt64Ty(*TheContext), ConstantInt::get(*TheContext, APInt(64, 0, false))}, // no methods
    {ptrT, Constant::getNullValue(ptrT)},
    {ptrT, Constant::getNullValue(ptrT)}
  };
  
  Value *classValue = callRuntimeFun("Class_create", ptrT, argTypes);
  Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
  callRuntimeFun("registerClass", Type::getVoidTy(*TheContext), {ptrT, ptrT}, {statePtr, classValue});
  return TypedValue(ObjectTypeSet(nilType), dynamicNil());
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DeftypeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(nilType);
}
