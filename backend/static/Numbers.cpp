#include "Numbers.h"
#include <math.h>
#include <functional>

using namespace std;
using namespace std::placeholders;
using namespace llvm;

// Numeric types priority: java.lang.Double > java.math.BigDecimal > clojure.lang.Ratio > clojure.lang.BigInt > java.lang.Long

ObjectTypeSet Numbers_generic_op_type(
  CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args,
  std::function<double(double, double)> doubleOp,
  std::function<void(mpz_t, const mpz_t, const mpz_t)> bigIntegerOp,
  std::function<int64_t(int64_t, int64_t)> integerOp
) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto left = args[0];
  auto right = args[1];

  if(left.getConstant() && right.getConstant()) {
    auto ltype = left.getConstant()->constantType;
    auto rtype = right.getConstant()->constantType;
    
    if (ltype == doubleType || rtype == doubleType) {
      double lval, rval;
      
      switch (ltype) {
        case doubleType:
          lval = dynamic_cast<ConstantDouble *>(left.getConstant())->value;
          break;
        case bigIntegerType:
          lval = mpz_get_d(dynamic_cast<ConstantBigInteger *>(left.getConstant())->value);
          break;
        case integerType:
          lval = static_cast<double>(dynamic_cast<ConstantInteger *>(left.getConstant())->value);
          break;
        default:
          break;
      }
      
      switch (rtype) {
        case doubleType:
          rval = dynamic_cast<ConstantDouble *>(right.getConstant())->value;
          break;
        case bigIntegerType:
          rval = mpz_get_d(dynamic_cast<ConstantBigInteger *>(right.getConstant())->value);
          break;
        case integerType:
          rval = static_cast<double>(dynamic_cast<ConstantInteger *>(right.getConstant())->value);
          break;
        default:
          break;
      }
      
      return ObjectTypeSet(doubleType, false, new ConstantDouble(doubleOp(lval, rval)));
    }
    
    if (ltype == bigIntegerType || rtype == bigIntegerType) {
      mpz_t lval, rval, opResult;
      
      switch (ltype) {
        case bigIntegerType:
          mpz_init_set(lval, dynamic_cast<ConstantBigInteger *>(left.getConstant())->value);
          break;
        case integerType:
          mpz_init_set_si(lval, dynamic_cast<ConstantInteger *>(left.getConstant())->value);
          break;
        default:
          break;
      }
      
      switch (rtype) {
        case bigIntegerType:
          mpz_init_set(rval, dynamic_cast<ConstantBigInteger *>(right.getConstant())->value);
          break;
        case integerType:
          mpz_init_set_si(rval, dynamic_cast<ConstantInteger *>(right.getConstant())->value);
          break;
        default:
          break;
      }
      
      mpz_init(opResult);
      bigIntegerOp(opResult, lval, rval);
      return ObjectTypeSet(bigIntegerType, false, new ConstantBigInteger(opResult));
    }
    
    if (ltype == integerType && rtype == integerType) {
      return ObjectTypeSet(integerType, false, new ConstantInteger(integerOp(
        dynamic_cast<ConstantInteger *>(left.getConstant())->value,
        dynamic_cast<ConstantInteger *>(right.getConstant())->value
      )));
    }
  }
  
  // REVIEW: O co w tym chodzi?
  if (left.isDetermined() && left.determinedType() == integerType && right == left) return ObjectTypeSet(integerType);
  if (left.isDetermined() && left.determinedType() == bigIntegerType && right == left) return ObjectTypeSet(bigIntegerType);
  return ObjectTypeSet(doubleType);
}

ObjectTypeSet Numbers_add_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x + y; },
    mpz_add,
    [](int64_t x, int64_t y) { return x + y; }
  );
}

ObjectTypeSet Numbers_compare_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node); 
  return ObjectTypeSet(booleanType);  
}

ObjectTypeSet Numbers_minus_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x - y; },
    mpz_sub,
    [](int64_t x, int64_t y) { return x - y; }
  );
}

ObjectTypeSet Numbers_multiply_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x * y; },
    mpz_mul,
    [](int64_t x, int64_t y) { return x * y; }
  );
}

ObjectTypeSet Numbers_divide_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  // TODO: zero checks, creating ratio out of division of two integers
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x / y; },
    mpz_cdiv_q,
    [](int64_t x, int64_t y) { return x / y; }
  );
}

TypedValue Numbers_add(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for add call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, right.second, "add_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, converted.second, "add_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, converted, "add_dd_tmp"));
    }
  }
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(converted.second, right.second, "add_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_add", ObjectTypeSet(bigIntegerType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_add", ObjectTypeSet(bigIntegerType), {left, converted});
    }
  }
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(converted, right.second, "add_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_add", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateAdd(left.second, right.second, "add_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for add call"), node);
}


TypedValue Numbers_gte(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  // TODO: It's not that obvious actually, what type to convert to
  // If we have (> 1e500 (BigInteger) 1e499 (BigInteger) 1e308 (Double)) [true], we can't convert arguments to double,
  // otherwise we would have (> ##Inf ##Inf 1e308) [false]
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto left = args[0]; 
  auto right = args[1];
 
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, right.second, "ge_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpSGE(left.second, right.second, "ge_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(converted, right.second, "ge_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, converted, "ge_dd_tmp"));
  }
  // TODO: generic version 
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_lt(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
 
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first == left.first) {
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, right.second, "ge_dd_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first == left.first) {
    /* TODO integer overflow or promotion to bigint */
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpSLT(left.second, right.second, "ge_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == integerType &&  right.first.isDetermined() && right.first.determinedType() == doubleType) {
    auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(converted, right.second, "ge_ii_tmp"));
  }
  
  if (left.first.isDetermined() &&  left.first.determinedType() == doubleType &&  right.first.isDetermined() && right.first.determinedType() == integerType) {
    auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
    return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, converted, "ge_dd_tmp"));
  }
  // TODO: generic version 
  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_minus(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for add call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, right.second, "sub_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, converted.second, "sub_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, converted, "sub_dd_tmp"));
    }
  }
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(converted.second, right.second, "sub_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_sub", ObjectTypeSet(bigIntegerType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_sub", ObjectTypeSet(bigIntegerType), {left, converted});
    }
  }
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(converted, right.second, "sub_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_sub", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateSub(left.second, right.second, "sub_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_multiply(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for add call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, right.second, "mul_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, converted.second, "mul_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, converted, "mul_dd_tmp"));
    }
  }
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(converted.second, right.second, "mul_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_mul", ObjectTypeSet(bigIntegerType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_mul", ObjectTypeSet(bigIntegerType), {left, converted});
    }
  }
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(converted, right.second, "mul_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_mul", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateMul(left.second, right.second, "mul_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for add call"), node);
}

TypedValue Numbers_divide(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  // TODO: Division by zero
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for add call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, right.second, "div_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, converted.second, "div_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, converted, "div_dd_tmp"));
    }
  }
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted.second, right.second, "div_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_div", ObjectTypeSet(bigIntegerType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_div", ObjectTypeSet(bigIntegerType), {left, converted});
    }
  }
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted, right.second, "div_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_div", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateSDiv(left.second, right.second, "div_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for add call"), node);
}

ObjectTypeSet Link_external_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  // TODO - should we apply math functions to constants and do more constant folding in the type system? */
  // TODO: Some of the math functions have gmp equivalents
  return ObjectTypeSet(doubleType);
}

TypedValue Link_external(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  /* We assume here that all the inputs and outputs are double. Should be true for most clib math functions. If not, a fully custom variant will be employed. */
  string tmp = signature.substr(0, signature.rfind(" "));
  string fname = tmp.substr(tmp.rfind("/") + 1);  
  if(fname == "abs") fname = "fabs";
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
    auto left = args[i];
    if (left.first.isDetermined()) {
      switch(left.first.determinedType()) {
        case doubleType:
          argsF.push_back(left.second);
          break;
        case integerType:
          argsF.push_back(gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i"));
          break;
        default:
          // TODO - generic types 
          break;
      }

    }
  }

  if (argsF.size() != args.size()) throw CodeGenerationException(string("Wrong types for calling") + fname, node);

  return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateCall(CalleeF, argsF, string("call_") + fname));
}

unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> getNumbersStaticFunctions() {
  unordered_map<string, vector<pair<string, pair<StaticCallType, StaticCall>>>> vals;
  vector<pair<string, pair<StaticCallType, StaticCall>>> addX, minusX, multiplyX, divideX, sinX, cosX, tanX, asinX, acosX, atanX, atan2X, expX, exp10X, powX, logX, log10X, logbX, log2X, sqrtX, cbrtX, hypotX, exp1mX, log1pX, exp2X, gte, abs, lt;

  // vector<string> numericTypes = {"D", "LM", "LR", "LI", "J"};
  vector<string> numericTypes = {"D", "LI", "J"};

  gte.push_back({"JJ", {&Numbers_compare_type, &Numbers_gte}});
  gte.push_back({"DJ", {&Numbers_compare_type, &Numbers_gte}});
  gte.push_back({"JD", {&Numbers_compare_type, &Numbers_gte}});
  gte.push_back({"DD", {&Numbers_compare_type, &Numbers_gte}});

  vals.insert({"clojure.lang.Numbers/gte", gte});

  lt.push_back({"JJ", {&Numbers_compare_type, &Numbers_lt}});
  lt.push_back({"DJ", {&Numbers_compare_type, &Numbers_lt}});
  lt.push_back({"JD", {&Numbers_compare_type, &Numbers_lt}});
  lt.push_back({"DD", {&Numbers_compare_type, &Numbers_lt}});

  vals.insert({"clojure.lang.Numbers/lt", lt});

  for (auto t1: numericTypes)
    for (auto t2: numericTypes) {
      addX.push_back({t1 + t2, {&Numbers_add_type, &Numbers_add}});
      minusX.push_back({t1 + t2, {&Numbers_minus_type, &Numbers_minus}});
      multiplyX.push_back({t1 + t2, {&Numbers_multiply_type, &Numbers_multiply}});
      divideX.push_back({t1 + t2, {&Numbers_divide_type, &Numbers_divide}});
    }
  vals.insert({"clojure.lang.Numbers/add", addX});
  vals.insert({"clojure.lang.Numbers/minus", minusX});
  vals.insert({"clojure.lang.Numbers/multiply", multiplyX});
  vals.insert({"clojure.lang.Numbers/divide", divideX}); 

  sinX.push_back({"J", {&Link_external_type, &Link_external}});
  sinX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/sin", sinX}); 

  cosX.push_back({"J", {&Link_external_type, &Link_external}});
  cosX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/cos", cosX}); 

  tanX.push_back({"J", {&Link_external_type, &Link_external}});
  tanX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/tan", tanX}); 

  asinX.push_back({"J", {&Link_external_type, &Link_external}});
  asinX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/asin", asinX}); 

  acosX.push_back({"J", {&Link_external_type, &Link_external}});
  acosX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/acos", acosX}); 

  atanX.push_back({"J", {&Link_external_type, &Link_external}});
  atanX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/atan", atanX}); 

  atan2X.push_back({"JJ", {&Link_external_type, &Link_external}});
  atan2X.push_back({"DJ", {&Link_external_type, &Link_external}});
  atan2X.push_back({"JD", {&Link_external_type, &Link_external}});
  atan2X.push_back({"DD", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/atan2", atan2X}); 

  expX.push_back({"J", {&Link_external_type, &Link_external}});
  expX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/exp", expX}); 

  exp2X.push_back({"J", {&Link_external_type, &Link_external}});
  exp2X.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/exp2", exp2X}); 

  exp10X.push_back({"J", {&Link_external_type, &Link_external}});
  exp10X.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/exp10", exp10X}); 

  powX.push_back({"JJ", {&Link_external_type, &Link_external}});
  powX.push_back({"DJ", {&Link_external_type, &Link_external}});
  powX.push_back({"JD", {&Link_external_type, &Link_external}});
  powX.push_back({"DD", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/pow", powX}); 

  logX.push_back({"J", {&Link_external_type, &Link_external}});
  logX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/log", logX}); 

  log10X.push_back({"J", {&Link_external_type, &Link_external}});
  log10X.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/log10", log10X}); 

  logbX.push_back({"J", {&Link_external_type, &Link_external}});
  logbX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/logb", logbX}); 

  log2X.push_back({"J", {&Link_external_type, &Link_external}});
  log2X.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/log2", log2X}); 

  sqrtX.push_back({"J", {&Link_external_type, &Link_external}});
  sqrtX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/sqrt", sqrtX}); 

  cbrtX.push_back({"J", {&Link_external_type, &Link_external}});
  cbrtX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/cbrt", cbrtX}); 

  hypotX.push_back({"JJ", {&Link_external_type, &Link_external}});
  hypotX.push_back({"DJ", {&Link_external_type, &Link_external}});
  hypotX.push_back({"JD", {&Link_external_type, &Link_external}});
  hypotX.push_back({"DD", {&Link_external_type, &Link_external}});
  vals.insert({"java.lang.Math/hypot", hypotX}); 

  exp1mX.push_back({"J", {&Link_external_type, &Link_external}});
  exp1mX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/exp1m", exp1mX}); 

  log1pX.push_back({"J", {&Link_external_type, &Link_external}});
  log1pX.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/log1p", log1pX}); 

  abs.push_back({"J", {&Link_external_type, &Link_external}});
  abs.push_back({"D", {&Link_external_type, &Link_external}});

  vals.insert({"java.lang.Math/abs", abs}); 


  return vals;
}
