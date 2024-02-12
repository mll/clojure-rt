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
  std::function<void(mpq_t, const mpq_t, const mpq_t)> ratioOp,
  std::function<void(mpz_t, const mpz_t, const mpz_t)> bigIntegerOp,
  std::function<int64_t(int64_t, int64_t)> integerOp,
  BOOL division
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
        case ratioType:
          lval = mpq_get_d(dynamic_cast<ConstantRatio *>(left.getConstant())->value);
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
        case ratioType:
          rval = mpq_get_d(dynamic_cast<ConstantRatio *>(right.getConstant())->value);
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
      
      if (division && !rval) {
        throw CodeGenerationException(string("Division by 0"), node);
      }
      return ObjectTypeSet(doubleType, false, new ConstantDouble(doubleOp(lval, rval)));
    }
    
    if (ltype == ratioType || rtype == ratioType) {
      mpq_t lval, rval;
      mpq_init(lval);
      mpq_init(rval);
      
      switch (ltype) {
        case ratioType:
          mpq_set(lval, dynamic_cast<ConstantRatio *>(left.getConstant())->value);
          break;
        case bigIntegerType:
          mpq_set_z(lval, dynamic_cast<ConstantBigInteger *>(left.getConstant())->value);
          break;
        case integerType:
          mpq_set_si(lval, dynamic_cast<ConstantInteger *>(left.getConstant())->value, 1);
          break;
        default:
          break;
      }
      
      switch (rtype) {
        case ratioType:
          mpq_set(rval, dynamic_cast<ConstantRatio *>(right.getConstant())->value);
          break;
        case bigIntegerType:
          mpq_set_z(rval, dynamic_cast<ConstantBigInteger *>(right.getConstant())->value);
          break;
        case integerType:
          mpq_set_si(rval, dynamic_cast<ConstantInteger *>(right.getConstant())->value, 1);
          break;
        default:
          break;
      }
      
      if (division && mpq_cmp_si(rval, 0, 1) == 0) {
        mpq_clear(lval);
        mpq_clear(rval);
        throw CodeGenerationException(string("Division by 0"), node);
      }
      ratioOp(lval, lval, rval);
      mpq_clear(rval);
      if (mpz_cmp_si(mpq_denref(lval), 1)) { // denominator not equal to 1
        return ObjectTypeSet(ratioType, false, new ConstantRatio(lval));
      } else {
        return ObjectTypeSet(bigIntegerType, false, new ConstantBigInteger(lval));
      }
    }
    
    if (ltype == bigIntegerType || rtype == bigIntegerType) {
      mpz_t lval, rval;
      
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
      
      if (division) {
        if (mpz_cmp_si(rval, 0) == 0) {
          mpz_clear(lval);
          mpz_clear(rval);
          throw CodeGenerationException(string("Division by 0"), node);
        }
        if (!mpz_divisible_p(lval, rval)) {
          return ObjectTypeSet(ratioType, false, new ConstantRatio(lval, rval));
        }
      }
      bigIntegerOp(lval, lval, rval);
      mpz_clear(rval);
      return ObjectTypeSet(bigIntegerType, false, new ConstantBigInteger(lval));
    }
    
    if (ltype == integerType && rtype == integerType) {
      int64_t lval = dynamic_cast<ConstantInteger *>(left.getConstant())->value,
              rval = dynamic_cast<ConstantInteger *>(right.getConstant())->value;
      if (division) {
        if (rval) {
          if (lval % rval) {
            return ObjectTypeSet(integerType, false, new ConstantInteger(integerOp(lval, rval)));
          } else {
            return ObjectTypeSet(ratioType, false, new ConstantRatio(lval, rval));
          }
        } else {
          throw CodeGenerationException(string("Division by 0"), node);
        }
      } else {
        return ObjectTypeSet(integerType, false, new ConstantInteger(integerOp(lval, rval)));
      }
    }
  }
  
  if (left.isDetermined() && left.determinedType() == integerType && right == left) {
    auto retVal = ObjectTypeSet(integerType);
    if (division) retVal.insert(ratioType);
    return retVal;
  }
  if (left.isDetermined() && left.determinedType() == bigIntegerType && right == left) {
    auto retVal = ObjectTypeSet(bigIntegerType);
    if (division) retVal.insert(ratioType);
    return retVal;
  }
  if (left.isDetermined() && left.determinedType() == ratioType && right == left) {
    auto retVal = ObjectTypeSet(bigIntegerType);
    retVal.insert(ratioType);
    return retVal;
  }
  return ObjectTypeSet(doubleType);
}

ObjectTypeSet Numbers_add_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x + y; },
    mpq_add,
    mpz_add,
    [](int64_t x, int64_t y) { return x + y; },
    false
  );
}

ObjectTypeSet Numbers_minus_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x - y; },
    mpq_sub,
    mpz_sub,
    [](int64_t x, int64_t y) { return x - y; },
    false
  );
}

ObjectTypeSet Numbers_multiply_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x * y; },
    mpq_mul,
    mpz_mul,
    [](int64_t x, int64_t y) { return x * y; },
    false
  );
}

ObjectTypeSet Numbers_divide_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  return Numbers_generic_op_type(gen, signature, node, args,
    [](double x, double y) { return x / y; },
    mpq_div,
    mpz_cdiv_q,
    [](int64_t x, int64_t y) { return x / y; },
    true
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
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(left.second, converted.second, "add_dd_tmp"));
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
  
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(converted.second, right.second, "add_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto typeSet = ObjectTypeSet(ratioType, true);
      typeSet.insert(bigIntegerType);
      return gen->callRuntimeFun("Ratio_add", typeSet, args);
    }
    // [BRR]: Ratio + Ratio (or Ratio - Ratio) might be Ratio or BigInteger, but
    // Ratio + BigInteger/Integer promoted to Ratio will always be Ratio because of invariant
    // ,,Ratio accesible by user should always be non-integer''
    // Either use ObjectTypeSet(ratioType, true) in return statements or typeSet from above
    // Lines related to this comment are marked with [BRR]
    // This is only for + and -, not for * or /
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_add", ObjectTypeSet(ratioType, true), {left, converted}); // [BRR]
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_add", ObjectTypeSet(ratioType, true), {left, converted}); // [BRR]
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFAdd(converted.second, right.second, "add_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_add", ObjectTypeSet(ratioType, true), {converted, right}); // [BRR]
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
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_add", ObjectTypeSet(ratioType, true), {converted, right}); // [BRR]
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

TypedValue Numbers_minus(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for minus call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, right.second, "sub_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(left.second, converted.second, "sub_dd_tmp"));
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
  
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(converted.second, right.second, "sub_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto typeSet = ObjectTypeSet(ratioType, true);
      typeSet.insert(bigIntegerType);
      return gen->callRuntimeFun("Ratio_sub", typeSet, args);
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_sub", ObjectTypeSet(ratioType, true), {left, converted}); // [BRR]
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_sub", ObjectTypeSet(ratioType, true), {left, converted}); // [BRR]
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFSub(converted.second, right.second, "sub_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_sub", ObjectTypeSet(ratioType, true), {converted, right}); // [BRR]
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
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_sub", ObjectTypeSet(ratioType, true), {converted, right}); // [BRR]
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_sub", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateSub(left.second, right.second, "sub_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for minus call"), node);
}

TypedValue Numbers_multiply(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for multiply call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, right.second, "mul_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(left.second, converted.second, "mul_dd_tmp"));
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
  
  auto typeSet = ObjectTypeSet(ratioType, true);
  typeSet.insert(bigIntegerType);
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(converted.second, right.second, "mul_dd_tmp"));
    }
    if (rtype == ratioType) {
      return gen->callRuntimeFun("Ratio_mul", typeSet, args);
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_mul", typeSet, {left, converted});
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_mul", typeSet, {left, converted});
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFMul(converted.second, right.second, "mul_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_mul", typeSet, {converted, right});
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
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_mul", typeSet, {converted, right});
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_mul", ObjectTypeSet(bigIntegerType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(integerType), gen->Builder->CreateMul(left.second, right.second, "mul_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for multiply call"), node);
}

TypedValue Numbers_divide(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  // TODO: Division by zero: unchecked for primitive types
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for divide call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, right.second, "div_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(left.second, converted.second, "div_dd_tmp"));
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
  
  auto typeSet = ObjectTypeSet(ratioType, true);
  typeSet.insert(bigIntegerType);
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted.second, right.second, "add_dd_tmp"));
    }
    if (rtype == ratioType) {
      return gen->callRuntimeFun("Ratio_div", typeSet, args);
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_div", typeSet, {left, converted});
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_div", typeSet, {left, converted});
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted.second, right.second, "div_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_div", typeSet, {converted, right});
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_div", typeSet, args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_div", typeSet, {left, converted});
    }
  }
  
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(doubleType), gen->Builder->CreateFDiv(converted, right.second, "div_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_div", typeSet, {converted, right});
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_div", typeSet, {converted, right});
    }
    if (rtype == integerType) {
      auto typeSet = ObjectTypeSet(ratioType, true);
      typeSet.insert(integerType);
      return gen->callRuntimeFun("Integer_div", typeSet, {left, right});
      TypedValue(typeSet, gen->Builder->CreateSDiv(left.second, right.second, "div_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for divide call"), node);
}

ObjectTypeSet Numbers_compare_type(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<ObjectTypeSet> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node); 
  return ObjectTypeSet(booleanType);  
}

TypedValue Numbers_gte(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for gte call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, right.second, "gte_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, converted.second, "gte_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, converted.second, "gte_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(left.second, converted, "gte_dd_tmp"));
    }
  }
  
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(converted.second, right.second, "gte_dd_tmp"));
    }
    if (rtype == ratioType) {
      return gen->callRuntimeFun("Ratio_gte", ObjectTypeSet(booleanType), args);
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_gte", ObjectTypeSet(booleanType), {left, converted});
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_gte", ObjectTypeSet(booleanType), {left, converted});
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(converted.second, right.second, "gte_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_gte", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_gte", ObjectTypeSet(booleanType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_gte", ObjectTypeSet(booleanType), {left, converted});
    }
  }
  
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOGE(converted, right.second, "gte_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_gte", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_gte", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpSGE(left.second, right.second, "gte_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for gte call"), node);
}

TypedValue Numbers_lt(CodeGenerator *gen, const string &signature, const Node &node, const std::vector<TypedValue> &args) {
  if (args.size() != 2) throw CodeGenerationException(string("Wrong number of arguments to a static call: ") + signature, node);
  auto left = args[0]; 
  auto right = args[1];
  
  if (!left.first.isDetermined() || !right.first.isDetermined()) {
    throw CodeGenerationException(string("Wrong types for lt call"), node);
  }
  
  auto ltype = left.first.determinedType();
  auto rtype = right.first.determinedType();
  
  if (ltype == doubleType) {
    if (rtype == doubleType) {
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, right.second, "lt_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, converted.second, "lt_dd_tmp"));
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {right});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, converted.second, "lt_dd_tmp"));
    }
    if (rtype == integerType) {
      auto converted = gen->Builder->CreateSIToFP(right.second, Type::getDoubleTy(*(gen->TheContext)), "convert_d_i");
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(left.second, converted, "lt_dd_tmp"));
    }
  }
  
  if (ltype == ratioType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(converted.second, right.second, "lt_dd_tmp"));
    }
    if (rtype == ratioType) {
      return gen->callRuntimeFun("Ratio_lt", ObjectTypeSet(booleanType), args);
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_lt", ObjectTypeSet(booleanType), {left, converted});
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {right});
      return gen->callRuntimeFun("Ratio_lt", ObjectTypeSet(booleanType), {left, converted});
    }
  }
  
  if (ltype == bigIntegerType) {
    if (rtype == doubleType) {
      auto converted = gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left});
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(converted.second, right.second, "lt_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromBigInteger", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_lt", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == bigIntegerType) {
      return gen->callRuntimeFun("BigInteger_lt", ObjectTypeSet(booleanType), args);
    }
    if (rtype == integerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {right});
      return gen->callRuntimeFun("BigInteger_lt", ObjectTypeSet(booleanType), {left, converted});
    }
  }
  
  if (ltype == integerType) {
    if (rtype == doubleType) {
      auto converted = gen->Builder->CreateSIToFP(left.second, Type::getDoubleTy(*(gen->TheContext)) , "convert_d_i");
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateFCmpOLT(converted, right.second, "lt_dd_tmp"));
    }
    if (rtype == ratioType) {
      auto converted = gen->callRuntimeFun("Ratio_createFromInt", ObjectTypeSet(ratioType), {left});
      return gen->callRuntimeFun("Ratio_lt", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == bigIntegerType) {
      auto converted = gen->callRuntimeFun("BigInteger_createFromInt", ObjectTypeSet(bigIntegerType), {left});
      return gen->callRuntimeFun("BigInteger_lt", ObjectTypeSet(booleanType), {converted, right});
    }
    if (rtype == integerType) {
      return TypedValue(ObjectTypeSet(booleanType), gen->Builder->CreateICmpSLT(left.second, right.second, "lt_ii_tmp"));
    }
  }

  throw CodeGenerationException(string("Wrong types for lt call"), node);
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
        case bigIntegerType:
          argsF.push_back(gen->callRuntimeFun("BigInteger_toDouble", ObjectTypeSet(doubleType), {left}).second);
          break;
        case ratioType:
          argsF.push_back(gen->callRuntimeFun("Ratio_toDouble", ObjectTypeSet(doubleType), {left}).second);
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
  vector<string> numericTypes = {"D", "LR", "LI", "J"};

  for (auto t1: numericTypes)
    for (auto t2: numericTypes) {
      addX.push_back({t1 + t2, {&Numbers_add_type, &Numbers_add}});
      minusX.push_back({t1 + t2, {&Numbers_minus_type, &Numbers_minus}});
      multiplyX.push_back({t1 + t2, {&Numbers_multiply_type, &Numbers_multiply}});
      divideX.push_back({t1 + t2, {&Numbers_divide_type, &Numbers_divide}});
      gte.push_back({t1 + t2, {&Numbers_compare_type, &Numbers_gte}});
      lt.push_back({t1 + t2, {&Numbers_compare_type, &Numbers_lt}});
    }
  vals.insert({"clojure.lang.Numbers/add", addX});
  vals.insert({"clojure.lang.Numbers/minus", minusX});
  vals.insert({"clojure.lang.Numbers/multiply", multiplyX});
  vals.insert({"clojure.lang.Numbers/divide", divideX});
  vals.insert({"clojure.lang.Numbers/gte", gte});
  vals.insert({"clojure.lang.Numbers/lt", lt});

  vector<string> math_1arg = {
    "java.lang.Math/sin", "java.lang.Math/cos", "java.lang.Math/tan", "java.lang.Math/asin",
    "java.lang.Math/acos", "java.lang.Math/atan",
    "java.lang.Math/exp", "java.lang.Math/exp2", "java.lang.Math/exp10",
    "java.lang.Math/log", "java.lang.Math/log10", "java.lang.Math/logb", "java.lang.Math/log2",
    "java.lang.Math/sqrt", "java.lang.Math/cbrt",
    "java.lang.Math/exp1m", "java.lang.Math/log1p", "java.lang.Math/abs"
  };
  
  vector<string> math_2arg = {
    "java.lang.Math/atan2", "java.lang.Math/pow", "java.lang.Math/hypot"
  };
  
  for (auto op: math_1arg) {
    vector<pair<string, pair<StaticCallType, StaticCall>>> opX;
    for (auto t: numericTypes) opX.push_back({t, {&Link_external_type, &Link_external}});
    vals.insert({op, opX});
  }
  
  for (auto op: math_2arg) {
    vector<pair<string, pair<StaticCallType, StaticCall>>> opX;
    for (auto t1: numericTypes)
      for (auto t2: numericTypes)
        opX.push_back({t1 + t2, {&Link_external_type, &Link_external}});
    vals.insert({op, opX});
  }

  return vals;
}
