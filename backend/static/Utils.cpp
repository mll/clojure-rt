#include "Utils.h"  
#include <math.h>

TypedValue Numbers_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first == doubleType && right.first == doubleType) {
    return TypedValue(booleanType, gen->Builder->CreateFCmpOEQ(left.second, right.second, "cmp_dd_tmp"));
  }
  
  if (left.first == integerType && right.first == integerType) {
        return TypedValue(booleanType, gen->Builder->CreateICmpEQ(left.second, right.second, "cmp_ii_tmp"));
  }
  
  if (left.first == integerType && right.first == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(booleanType, gen->Builder->CreateFCmpOEQ(converted, right.second, "cmp_dd_tmp"));
  }
  
  if (left.first == doubleType && right.first == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(booleanType, gen->Builder->CreateFCmpOEQ(left.second, converted, "cmp_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for compare call"), node);
}



TypedValue Utils_compare(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  // if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  // auto left = args[0];
  // auto right = args[1];
  
  // if (left.first == doubleType && right.first == doubleType) {
  //   return TypedValue(doubleType, gen->Builder->CreateFAdd(left.second, right.second, "add_dd_tmp"));
  // }
  
  // if (left.first == integerType && right.first == integerType) {
  //   /* TODO integer overflow or promotion to bigint */
  //   return TypedValue(integerType, gen->Builder->CreateAdd(left.second, right.second, "add_ii_tmp"));
  // }
  
  // if (left.first == integerType && right.first == doubleType) {
  //   auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
  //   return TypedValue(doubleType, gen->Builder->CreateFAdd(converted, right.second, "add_dd_tmp"));
  // }
  
  // if (left.first == doubleType && right.first == integerType) {
  //   auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
  //   return TypedValue(doubleType, gen->Builder->CreateFAdd(left.second, converted, "add_dd_tmp"));
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

  vals.insert({"clojure.lang.Util/equiv", equiv});  


  // vals.insert({"clojure.lang.Util/equiv DD", {ObjectTypeSet(booleanType), &Numbers_compare}});
  // vals.insert({"clojure.lang.Util/equiv DJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  // vals.insert({"clojure.lang.Util/equiv JJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  // vals.insert({"clojure.lang.Util/equiv JD", {ObjectTypeSet(booleanType), &Numbers_compare}});

  return vals;
}
