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

map<string, StaticCall> getNumbersStaticFunctions() {
  map<string, StaticCall> vals;
  vals.insert({"clojure.lang.Numbers/add DD", &Numbers_add});
  vals.insert({"clojure.lang.Numbers/add JJ", &Numbers_add});
  vals.insert({"clojure.lang.Numbers/add DJ", &Numbers_add});
  vals.insert({"clojure.lang.Numbers/add JD", &Numbers_add});

  vals.insert({"clojure.lang.Numbers/minus DD", &Numbers_minus});
  vals.insert({"clojure.lang.Numbers/minus JJ", &Numbers_minus});
  vals.insert({"clojure.lang.Numbers/minus DJ", &Numbers_minus});
  vals.insert({"clojure.lang.Numbers/minus JD", &Numbers_minus});

  vals.insert({"clojure.lang.Numbers/multiply DD", &Numbers_multiply});
  vals.insert({"clojure.lang.Numbers/multiply JJ", &Numbers_multiply});
  vals.insert({"clojure.lang.Numbers/multiply DJ", &Numbers_multiply});
  vals.insert({"clojure.lang.Numbers/multiply JD", &Numbers_multiply});

  vals.insert({"clojure.lang.Numbers/divide DD", &Numbers_divide});
  vals.insert({"clojure.lang.Numbers/divide JJ", &Numbers_divide});
  vals.insert({"clojure.lang.Numbers/divide DJ", &Numbers_divide});
  vals.insert({"clojure.lang.Numbers/divide JD", &Numbers_divide});

  vals.insert({"java.lang.Math/sin D", &Link_external});
  vals.insert({"java.lang.Math/sin J", &Link_external});

  vals.insert({"java.lang.Math/cos D", &Link_external});
  vals.insert({"java.lang.Math/cos J", &Link_external});

  vals.insert({"java.lang.Math/tan D", &Link_external});
  vals.insert({"java.lang.Math/tan J", &Link_external});

  vals.insert({"java.lang.Math/asin D", &Link_external});
  vals.insert({"java.lang.Math/asin J", &Link_external});

  vals.insert({"java.lang.Math/acos D", &Link_external});
  vals.insert({"java.lang.Math/acos J", &Link_external});

  vals.insert({"java.lang.Math/atan D", &Link_external});
  vals.insert({"java.lang.Math/atan J", &Link_external});

  vals.insert({"java.lang.Math/atan2 DD", &Link_external});
  vals.insert({"java.lang.Math/atan2 JJ", &Link_external});
  vals.insert({"java.lang.Math/atan2 DJ", &Link_external});
  vals.insert({"java.lang.Math/atan2 JD", &Link_external});


  vals.insert({"java.lang.Math/exp D", &Link_external});
  vals.insert({"java.lang.Math/exp J", &Link_external});

  vals.insert({"java.lang.Math/exp2 D", &Link_external});
  vals.insert({"java.lang.Math/exp2 J", &Link_external});

  vals.insert({"java.lang.Math/exp10 D", &Link_external});
  vals.insert({"java.lang.Math/exp10 J", &Link_external});

  vals.insert({"java.lang.Math/pow DD", &Link_external});
  vals.insert({"java.lang.Math/pow JJ", &Link_external});
  vals.insert({"java.lang.Math/pow DJ", &Link_external});
  vals.insert({"java.lang.Math/pow JD", &Link_external});
  
  vals.insert({"java.lang.Math/log D", &Link_external});
  vals.insert({"java.lang.Math/log J", &Link_external});

  vals.insert({"java.lang.Math/log10 D", &Link_external});
  vals.insert({"java.lang.Math/log10 J", &Link_external});

  vals.insert({"java.lang.Math/logb D", &Link_external});
  vals.insert({"java.lang.Math/logb J", &Link_external});

  vals.insert({"java.lang.Math/log2 D", &Link_external});
  vals.insert({"java.lang.Math/log2 J", &Link_external});

  vals.insert({"java.lang.Math/sqrt D", &Link_external});
  vals.insert({"java.lang.Math/sqrt J", &Link_external});

  vals.insert({"java.lang.Math/cbrt D", &Link_external});
  vals.insert({"java.lang.Math/cbrt J", &Link_external});

  vals.insert({"java.lang.Math/hypot DD", &Link_external});
  vals.insert({"java.lang.Math/hypot JJ", &Link_external});

  vals.insert({"java.lang.Math/hypot DJ", &Link_external});
  vals.insert({"java.lang.Math/hypot JD", &Link_external});

  vals.insert({"java.lang.Math/expm1 D", &Link_external});
  vals.insert({"java.lang.Math/expm1 J", &Link_external});

  vals.insert({"java.lang.Math/log1p D", &Link_external});
  vals.insert({"java.lang.Math/log1p J", &Link_external});

  // vals.insert({"java.lang.Math/M_E ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_LOG2E ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_LOG10E ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_LN2 ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_LN10 ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_PI ", &Link_external_const});
  // vals.insert({"java.lang.Math/M_LN2 ", &Link_external_const});




  return vals;
}
