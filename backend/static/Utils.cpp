#include "Utils.h"  
#include <math.h>

TypedValue Numbers_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOEQ(left.second, right.second, "cmp_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
        return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpEQ(left.second, right.second, "cmp_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOEQ(converted, right.second, "cmp_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOEQ(left.second, converted, "cmp_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for compare call"), node);
}




TypedValue Utils_staticTrue(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  return gen->staticTrue();
}

TypedValue Utils_staticFalse(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  return gen->staticFalse();
}


TypedValue Utils_compare_lo_ro(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_compare_lo(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_compare_ro(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Utils_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  // if(!(left.first == right.first)) return gen->staticFalse();
  // if(left.first.isDetermined()) throw CodeGenerationException(string("Indetermined arguments: ") + left.first.toString(), node);
  // switch(left.first.determinedValue()) {
    

  // }

  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}



unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> getUtilsStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> vals;
  vector<pair<string, pair<ObjectTypeSet, StaticCall>>> equiv;

  equiv.push_back({"JJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  equiv.push_back({"DJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  equiv.push_back({"JD", {ObjectTypeSet(booleanType), &Numbers_compare}});
  equiv.push_back({"DD", {ObjectTypeSet(booleanType), &Numbers_compare}});

  equiv.push_back({"JLO", {ObjectTypeSet(booleanType), &Numbers_compare_ro}});
  equiv.push_back({"LOJ", {ObjectTypeSet(booleanType), &Numbers_compare_lo}});

  equiv.push_back({"LOD", {ObjectTypeSet(booleanType), &Numbers_compare_lo}});
  equiv.push_back({"DLO", {ObjectTypeSet(booleanType), &Numbers_compare_ro}});

  equiv.push_back({"LOLO", {ObjectTypeSet(booleanType), &Utils_compare_lo_ro}});
  vector<string> types {"LS", "LV", "LL", "LY", "LK", "LR", "LH", "LN"};
  for(auto type1: types)
    for(auto type2: types) {
      equiv.push_back({type1+type2, {ObjectTypeSet(booleanType), &Utils_compare}});
    }
  vals.insert({"clojure.lang.Util/equiv", equiv});  

  return vals;
}
