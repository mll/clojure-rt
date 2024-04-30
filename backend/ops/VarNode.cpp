#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto type = getType(node, typeRestrictions);
  Var *var = TheProgramme->DefinedVarsByName.find(name)->second; // getType verifies that it exists
  auto ptrT = Type::getInt8Ty(*TheContext)->getPointerTo();
  Value *varPtr = Builder->CreateBitOrPointerCast(ConstantInt::get(Type::getInt64Ty(*TheContext), APInt(64, (uint64_t) var, false)), ptrT);
  dynamicRetain(varPtr);
  Value *varValue = callRuntimeFun("Var_deref", ptrT, {ptrT}, {varPtr});
  return TypedValue(type, varValue);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const VarNode &subnode, const ObjectTypeSet &typeRestrictions) {
  string name = subnode.var().substr(2);
  auto found = TheProgramme->DefinedVarsByName.find(name);
  if (found == TheProgramme->DefinedVarsByName.end()) throw CodeGenerationException(string("Undeclared var: ") + name, node);
  return ObjectTypeSet::all();
}
