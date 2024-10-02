#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  auto varName = subnode.var().substr(2);
  Var *var = TheProgramme->getVarByName(varName);
  if (!var) throw CodeGenerationException("Unable to resolve var: " + varName + " in this context", node);
  Value *varPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) var, false)), ptrT);
  return TypedValue(ObjectTypeSet(varType), varPtr);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const TheVarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  return ObjectTypeSet(varType);
}
