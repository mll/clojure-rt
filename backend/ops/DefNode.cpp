#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto type = getType(node, subnode, typeRestrictions);
  auto found = TheProgramme->getVar(name).first;
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  Value *varPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) found, false)), ptrT);

  if (subnode.has_init()) {
    Value *initValue = box(codegen(subnode.init(), ObjectTypeSet::all())).second;
    dynamicRetain(varPtr);
    Value *nil = callRuntimeFun("Var_bindRoot", ptrT, {ptrT, ptrT}, {varPtr, initValue});
    dynamicRelease(nil, false);
  }
  dynamicRetain(varPtr); // var can't be dropped!
  return TypedValue(type, varPtr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const DefNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto name = subnode.var().substr(2);
  if (!typeRestrictions.contains(varType)) throw CodeGenerationException(string("Def cannot be used here because it returns var: ") + name, node);
  TheProgramme->getVar(name); // create var if it does not exist, this is intended!
  return ObjectTypeSet(varType);
}
