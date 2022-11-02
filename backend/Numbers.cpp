#include "Numbers.h"  
#include <math.h>

TypedValue Numbers_add(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first == doubleType && right.first == doubleType) {
    return TypedValue(doubleType, gen->Builder->CreateFAdd(left.second, right.second, "add_dd_tmp"));
  }
  
  if (left.first == integerType && right.first == integerType) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(integerType, gen->Builder->CreateAdd(left.second, right.second, "add_ii_tmp"));
  }
  
  if (left.first == integerType && right.first == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFAdd(converted, right.second, "add_dd_tmp"));
  }
  
  if (left.first == doubleType && right.first == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFAdd(left.second, converted, "add_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

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
    return TypedValue(doubleType, gen->Builder->CreateFCmpOEQ(left.second, converted, "cmp_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_minus(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first == doubleType && right.first == doubleType) {
    return TypedValue(doubleType, gen->Builder->CreateFSub(left.second, right.second, "sub_dd_tmp"));
  }
  
  if (left.first == integerType && right.first == integerType) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(integerType, gen->Builder->CreateSub(left.second, right.second, "sub_ii_tmp"));
  }
  
  if (left.first == integerType && right.first == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFSub(converted, right.second, "sub_dd_tmp"));
  }
  
  if (left.first == doubleType && right.first == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFSub(left.second, converted, "sub_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for minus call"), node);
}

TypedValue Numbers_multiply(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first == doubleType && right.first == doubleType) {
    return TypedValue(doubleType, gen->Builder->CreateFMul(left.second, right.second, "mul_dd_tmp"));
  }
  
  if (left.first == integerType && right.first == integerType) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(integerType, gen->Builder->CreateMul(left.second, right.second, "mul_ii_tmp"));
  }
  
  if (left.first == integerType && right.first == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFMul(converted, right.second, "mul_dd_tmp"));
  }
  
  if (left.first == doubleType && right.first == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFMul(left.second, converted, "mul_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for minus call"), node);
}


TypedValue Numbers_divide(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  /*TODO - naive implementation. Null denominator checks have to be performed as well as Ratio type (if both divisor and divider are ints or bigints) */

  auto left = args[0];
  auto right = args[1];
  
  if (left.first == doubleType && right.first == doubleType) {
    return TypedValue(doubleType, gen->Builder->CreateFDiv(left.second, right.second, "div_dd_tmp"));
  }
  
  if (left.first == integerType && right.first == integerType) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(integerType, gen->Builder->CreateSDiv(left.second, right.second, "div_ii_tmp"));
  }
  
  if (left.first == integerType && right.first == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFDiv(converted, right.second, "div_dd_tmp"));
  }
  
  if (left.first == doubleType && right.first == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(doubleType, gen->Builder->CreateFDiv(left.second, converted, "div_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for divide call"), node);
}

TypedValue Link_external(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  /* We assume here that all the inputs and outputs are double. Should be true for most clib math functions. If not, a fully custom variant will be employed. */
  string tmp = signature.substr(0, signature.rfind(" "));
  string fname = tmp.substr(tmp.rfind("/") + 1);  
  
  Function *CalleeF = gen->TheModule->getFunction(fname);
  if (!CalleeF) {
    std::vector<Type*> Doubles(args.size(), Type::getDoubleTy(*(gen->TheContext)));
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*(gen->TheContext)), Doubles, false);
    CalleeF = Function::Create(FT, Function::ExternalLinkage, fname, gen->TheModule.get());
  }

  if (!CalleeF) throw CodeGenerationException(string("Unable to find ") + fname + " - do we link libc?", node);
  if (CalleeF->arg_size() != args.size()) throw CodeGenerationException(string("Argument count mismatch"), node);

  
  vector <Value *> argsF;
  for(int i=0; i< args.size(); i++) {
    auto left = args[0];
    if (left.first == doubleType) argsF.push_back(left.second);
    if (left.first == integerType) argsF.push_back(gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i"));
  }

  if (argsF.size() != args.size()) throw CodeGenerationException(string("Wrong types for calling") + fname, node);

  return TypedValue(doubleType, gen->Builder->CreateCall(CalleeF, argsF, string("call_") + fname));
}

map<string, pair<ObjectTypeSet, StaticCall>> getNumbersStaticFunctions() {
  map<string, pair<ObjectTypeSet, StaticCall>> vals;
  vals.insert({"clojure.lang.Numbers/add DD", {ObjectTypeSet(doubleType), &Numbers_add}});
  vals.insert({"clojure.lang.Numbers/add JJ", {ObjectTypeSet(doubleType), &Numbers_add}});
  vals.insert({"clojure.lang.Numbers/add DJ", {ObjectTypeSet(doubleType), &Numbers_add}});
  vals.insert({"clojure.lang.Numbers/add JD", {ObjectTypeSet(doubleType), &Numbers_add}});

  vals.insert({"clojure.lang.Numbers/minus DD", {ObjectTypeSet(doubleType), &Numbers_minus}});
  vals.insert({"clojure.lang.Numbers/minus JJ", {ObjectTypeSet(doubleType), &Numbers_minus}});
  vals.insert({"clojure.lang.Numbers/minus DJ", {ObjectTypeSet(doubleType), &Numbers_minus}});
  vals.insert({"clojure.lang.Numbers/minus JD", {ObjectTypeSet(doubleType), &Numbers_minus}});

  vals.insert({"clojure.lang.Numbers/multiply DD", {ObjectTypeSet(doubleType), &Numbers_multiply}});
  vals.insert({"clojure.lang.Numbers/multiply JJ", {ObjectTypeSet(doubleType), &Numbers_multiply}});
  vals.insert({"clojure.lang.Numbers/multiply DJ", {ObjectTypeSet(doubleType), &Numbers_multiply}});
  vals.insert({"clojure.lang.Numbers/multiply JD", {ObjectTypeSet(doubleType), &Numbers_multiply}});

  vals.insert({"clojure.lang.Numbers/divide DD", {ObjectTypeSet(doubleType), &Numbers_divide}});
  vals.insert({"clojure.lang.Numbers/divide JJ", {ObjectTypeSet(doubleType), &Numbers_divide}});
  vals.insert({"clojure.lang.Numbers/divide DJ", {ObjectTypeSet(doubleType), &Numbers_divide}});
  vals.insert({"clojure.lang.Numbers/divide JD", {ObjectTypeSet(doubleType), &Numbers_divide}});

  vals.insert({"java.lang.Math/sin D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/sin J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/cos D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/cos J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/tan D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/tan J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/asin D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/asin J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/acos D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/acos J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/atan D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/atan J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/atan2 DD", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/atan2 JJ", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/atan2 DJ", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/atan2 JD", {ObjectTypeSet(doubleType), &Link_external}});


  vals.insert({"java.lang.Math/exp D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/exp J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp2 D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/exp2 J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp10 D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/exp10 J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/pow DD", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/pow JJ", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/pow DJ", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/pow JD", {ObjectTypeSet(doubleType), &Link_external}});
  
  vals.insert({"java.lang.Math/log D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/log J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log10 D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/log10 J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/logb D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/logb J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log2 D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/log2 J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/sqrt D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/sqrt J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/cbrt D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/cbrt J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/hypot DD", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/hypot JJ", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/hypot DJ", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/hypot JD", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/expm1 D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/expm1 J", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log1p D", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/log1p J", {ObjectTypeSet(doubleType), &Link_external}});


  vals.insert({"clojure.lang.Util/equiv DD", {ObjectTypeSet(booleanType), &Numbers_compare}});
  vals.insert({"clojure.lang.Util/equiv DJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  vals.insert({"clojure.lang.Util/equiv JJ", {ObjectTypeSet(booleanType), &Numbers_compare}});
  vals.insert({"clojure.lang.Util/equiv JD", {ObjectTypeSet(booleanType), &Numbers_compare}});







  // vals.insert({"java.lang.Math/M_E ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_LOG2E ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_LOG10E ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_LN2 ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_LN10 ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_PI ", {ObjectTypeSet(doubleType), &Link_external_const}});
  // vals.insert({"java.lang.Math/M_LN2 ", {ObjectTypeSet(doubleType), &Link_external_const}});




  return vals;
}
