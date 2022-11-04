#include "Utils.h"  
#include <math.h>

TypedValue Utils_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedNode> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = gen->codegen(args[0].second, args[0].first);
  auto right = gen->codegen(args[1].second, args[1].first);
    
  if(left.first.isDetermined() && right.first.isDetermined()) {
    auto leftType = left.first.determinedType();
    auto rightType = right.first.determinedType();
    
    /* TODO - This is a strong assumption, probably would have to become weaker as we analyse ratios and big integers */
    if(leftType != rightType) return gen->staticFalse();
    
    cout << "ZOOOOO" <<endl;
    
    switch(leftType) {
    case integerType:
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpEQ(left.second, right.second, "cmp_jj_tmp"));
    case doubleType: 
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOEQ(left.second, right.second, "cmp_dd_tmp"));
    case booleanType:
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpEQ(left.second, right.second, "cmp_zz_tmp"));
    default:
      break;
    }
  }
  
  if((!left.first.isDetermined() && !right.first.isDetermined()) ||
     (left.first.isDetermined() && right.first.isDetermined())) {
    
    vector<Type *> argTypes = { gen->dynamicBoxedType(), gen->dynamicBoxedType() };
    vector<Value *> argss = { left.second, right.second };
    Value *int8bool = gen->callRuntimeFun("equals", Type::getInt8Ty(*(gen->TheContext)), argTypes, argss); 
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateIntCast(int8bool, gen->dynamicUnboxedType(booleanType), false));
  }
  
  auto determinedType = left.first.isDetermined() ? left.first.determinedType() : right.first.determinedType();
  auto determinedValue = left.first.isDetermined() ? left.second : right.second;
  auto otherValue = left.first.isDetermined() ? right.second : left.second;
  string cmpName;
  Type *determinedLLVMType = determinedValue->getType();
  
  switch(determinedType) {
  case integerType:
    cmpName = "unboxedEqualsInteger";
    break;
  case doubleType:
    cmpName = "unboxedEqualsDouble";
    break;
  case booleanType:
    cmpName = "unboxedEqualsBoolean";
    determinedLLVMType = Type::getInt8Ty(*(gen->TheContext));
    determinedValue = gen->Builder->CreateIntCast(determinedValue, determinedLLVMType, false);
    break;
  default:
    cmpName = "equals";
  }
  vector<Type *> argTypes = { gen->dynamicBoxedType(), determinedLLVMType };
  vector<Value *> argss = { otherValue, determinedValue };
  Value *int8bool = gen->callRuntimeFun(cmpName, Type::getInt8Ty(*(gen->TheContext)), argTypes, argss); 
  return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateIntCast(int8bool, gen->dynamicUnboxedType(booleanType), false));
}

unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> getUtilsStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> vals;
  vector<pair<string, pair<ObjectTypeSet, StaticCall>>> equiv;

  vector<string> types {"J", "D", "Z", "LS", "LV", "LL", "LY", "LK", "LR", "LH", "LN", "LO"};
  for(auto type1: types)
    for(auto type2: types) {
      equiv.push_back({type1+type2, {ObjectTypeSet(booleanType), &Utils_compare}});
    }
  vals.insert({"clojure.lang.Util/equiv", equiv});  

  return vals;
}
