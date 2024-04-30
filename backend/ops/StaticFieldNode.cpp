#include "../codegen.h"  

using namespace std;
using namespace llvm;

extern "C" {
  void retain(void *obj);
} 

TypedValue CodeGenerator::codegen(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  auto staticFieldType = getType(node, subnode, typeRestrictions);
  Value *className = dynamicString(subnode.class_().c_str());
  dynamicRetain(className);
  Value *statePtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) &*TheProgramme, false)), ptrT);
  Value *classValue = callRuntimeFun("getClassByName", ptrT, {ptrT, ptrT}, {statePtr, className}); // should not be null, verified in getType
  Value *staticFieldKeyword = dynamicKeyword(subnode.field().c_str());
  dynamicRetain(classValue);
  Value *staticFieldIndex = callRuntimeFun("Class_staticFieldIndex", Type::getInt64Ty(*TheContext), {ptrT, ptrT}, {classValue, staticFieldKeyword}); // should not be -1, verified in getType
  Value *staticField = callRuntimeFun("Class_getIndexedStaticField", ptrT, {ptrT, Type::getInt64Ty(*TheContext)}, {classValue, staticFieldIndex});
  return TypedValue(staticFieldType, staticField);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const StaticFieldNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto className = subnode.class_();
  Class *_class = TheProgramme->getClass(TheProgramme->getClassId(className));
  if (!_class) throw CodeGenerationException(string("Class ") + className + string(" not found"), node);
  Keyword *field = Keyword_create(String_createDynamicStr(subnode.field().c_str()));
  retain(_class);
  int64_t staticFieldIndex = Class_staticFieldIndex(_class, field);
  if (staticFieldIndex == -1) throw CodeGenerationException(string("Class ") + className + string(" does not have static field ") + subnode.field(), node);
  return ObjectTypeSet::all();
}
