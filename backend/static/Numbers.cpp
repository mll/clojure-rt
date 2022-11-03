#include "Numbers.h"  
#include <math.h>

TypedValue Numbers_add(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, right.second, "add_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateAdd(left.second, right.second, "add_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(converted, right.second, "add_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, converted, "add_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for add call"), node);
}


TypedValue Numbers_minus(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateSub(left.second, right.second, "sub_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(converted, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, converted, "sub_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for minus call"), node);
}

TypedValue Numbers_multiply(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateMul(left.second, right.second, "sub_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(converted, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, converted, "sub_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for minus call"), node);
}


TypedValue Numbers_divide(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  /*TODO - naive implementation. Null denominator checks have to be performed as well as Ratio type (if both divisor and divider are ints or bigints) */

  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0];
  auto right = args[1];
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateSDiv(left.second, right.second, "sub_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted, right.second, "sub_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, converted, "sub_dd_tmp"));
  }
  
  throw CodeGenerationException(string("Wrong types for minus call"), node);
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

  return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateCall(CalleeF, argsF, string("call_") + fname));
}

unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> getNumbersStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<ObjectTypeSet, StaticCall>>>> vals;
  vector<pair<string, pair<ObjectTypeSet, StaticCall>>> addX, minusX, multiplyX, divideX, sinX, cosX, tanX, asinX, acosX, atanX, atan2X, expX, exp10X, powX, logX, log10X, logbX, log2X, sqrtX, cbrtX, hypotX, exp1mX, log1pX, exp2X;

  addX.push_back({"JJ", {ObjectTypeSet(integerType), &Numbers_add}});
  addX.push_back({"DJ", {ObjectTypeSet(doubleType), &Numbers_add}});
  addX.push_back({"JD", {ObjectTypeSet(doubleType), &Numbers_add}});
  addX.push_back({"DD", {ObjectTypeSet(doubleType), &Numbers_add}});

  vals.insert({"clojure.lang.Numbers/add", addX});

  minusX.push_back({"JJ", {ObjectTypeSet(integerType), &Numbers_minus}});
  minusX.push_back({"DJ", {ObjectTypeSet(doubleType), &Numbers_minus}});
  minusX.push_back({"JD", {ObjectTypeSet(doubleType), &Numbers_minus}});
  minusX.push_back({"DD", {ObjectTypeSet(doubleType), &Numbers_minus}});

  vals.insert({"clojure.lang.Numbers/minus", minusX});

  multiplyX.push_back({"JJ", {ObjectTypeSet(integerType), &Numbers_multiply}});
  multiplyX.push_back({"DJ", {ObjectTypeSet(doubleType), &Numbers_multiply}});
  multiplyX.push_back({"JD", {ObjectTypeSet(doubleType), &Numbers_multiply}});
  multiplyX.push_back({"DD", {ObjectTypeSet(doubleType), &Numbers_multiply}});

  vals.insert({"clojure.lang.Numbers/multiply", multiplyX}); 

  divideX.push_back({"JJ", {ObjectTypeSet(integerType), &Numbers_divide}});
  divideX.push_back({"DJ", {ObjectTypeSet(doubleType), &Numbers_divide}});
  divideX.push_back({"JD", {ObjectTypeSet(doubleType), &Numbers_divide}});
  divideX.push_back({"DD", {ObjectTypeSet(doubleType), &Numbers_divide}});

  vals.insert({"clojure.lang.Numbers/divide", divideX}); 

  sinX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  sinX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/sin", sinX}); 

  cosX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  cosX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/cos", cosX}); 

  tanX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  tanX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/tan", tanX}); 

  asinX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  asinX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/asin", asinX}); 

  acosX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  acosX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/acos", acosX}); 

  atanX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  atanX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/atan", atanX}); 

  atan2X.push_back({"JJ", {ObjectTypeSet(doubleType), &Link_external}});
  atan2X.push_back({"DJ", {ObjectTypeSet(doubleType), &Link_external}});
  atan2X.push_back({"JD", {ObjectTypeSet(doubleType), &Link_external}});
  atan2X.push_back({"DD", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/atan2", atan2X}); 

  expX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  expX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp", expX}); 

  exp2X.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  exp2X.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp2", exp2X}); 

  exp10X.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  exp10X.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp10", exp10X}); 

  powX.push_back({"JJ", {ObjectTypeSet(doubleType), &Link_external}});
  powX.push_back({"DJ", {ObjectTypeSet(doubleType), &Link_external}});
  powX.push_back({"JD", {ObjectTypeSet(doubleType), &Link_external}});
  powX.push_back({"DD", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/pow", powX}); 

  logX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  logX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log", logX}); 

  log10X.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  log10X.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log10", log10X}); 

  logbX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  logbX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/logb", logbX}); 

  log2X.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  log2X.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log2", log2X}); 

  sqrtX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  sqrtX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/sqrt", sqrtX}); 

  cbrtX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  cbrtX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/cbrt", cbrtX}); 

  hypotX.push_back({"JJ", {ObjectTypeSet(doubleType), &Link_external}});
  hypotX.push_back({"DJ", {ObjectTypeSet(doubleType), &Link_external}});
  hypotX.push_back({"JD", {ObjectTypeSet(doubleType), &Link_external}});
  hypotX.push_back({"DD", {ObjectTypeSet(doubleType), &Link_external}});
  vals.insert({"java.lang.Math/hypot", hypotX}); 

  exp1mX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  exp1mX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/exp1m", exp1mX}); 

  log1pX.push_back({"J", {ObjectTypeSet(doubleType), &Link_external}});
  log1pX.push_back({"D", {ObjectTypeSet(doubleType), &Link_external}});

  vals.insert({"java.lang.Math/log1p", log1pX}); 

  return vals;
}
