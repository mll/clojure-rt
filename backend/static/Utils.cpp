#include "Utils.h"  
#include <math.h>

using namespace std;
using namespace llvm;


ObjectTypeSet Utils_compare_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto left = args[0];
  auto right = args[1];
  
  if(left.isDetermined() && right.isDetermined()) {
    auto leftType = left.determinedType();
    auto rightType = right.determinedType();
    
    /* TODO - This is a strong assumption, probably would have to become weaker as we analyse ratios and big integers */
    if(leftType != rightType) return ObjectTypeSet(booleanType, false, new ConstantBoolean(false));
    
    if(left.getConstant() && right.getConstant()) return ObjectTypeSet(booleanType, false, new ConstantBoolean(left.getConstant()->equals(right.getConstant())));
  }
  
  return ObjectTypeSet(booleanType);  
}

TypedValue Utils_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
    
  if(left.first.isDetermined() && right.first.isDetermined()) {
    auto leftType = left.first.determinedType();
    auto rightType = right.first.determinedType();
    
    /* TODO - This is a strong assumption, probably would have to become weaker as we analyse ratios and big integers */
    if(leftType != rightType) {
      if(!left.first.isScalar()) gen->dynamicRelease(left.second, false);
      if(!right.first.isScalar()) gen->dynamicRelease(right.second, false);
      return gen->staticFalse();
    }
    
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
    auto retVal = TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateIntCast(int8bool, gen->dynamicUnboxedType(booleanType), false));
    gen->dynamicRelease(left.second, false);
    gen->dynamicRelease(right.second, false);    
    return retVal;
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
  auto retVal = TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateIntCast(int8bool, gen->dynamicUnboxedType(booleanType), false));
  if(!left.first.isScalar()) gen->dynamicRelease(left.second, false);
  if(!right.first.isScalar()) gen->dynamicRelease(right.second, false);

  return retVal;
}

unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> getUtilsStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> vals;
  vector<pair<string, pair<StaticCallType, StaticCall>>> equiv;

  vector<string> types = ObjectTypeSet::allTypes();
  for(auto type1: types)
    for(auto type2: types) {
      equiv.push_back({type1+type2, {&Utils_compare_type, &Utils_compare}});
    }
  vals.insert({"clojure.lang.Util/equiv", equiv});  

  return vals;
}
