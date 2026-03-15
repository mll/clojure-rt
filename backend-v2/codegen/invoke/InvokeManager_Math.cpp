#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "InvokeManager.h"
#include "llvm/IR/MDBuilder.h"

using namespace llvm;
using namespace std;

namespace rt {

void registerMathIntrinsics(InvokeManager &mgr) {
  auto &typeIntrinsics = mgr.typeIntrinsics;
  auto &intrinsics = mgr.intrinsics;
  auto &genericIntrinsics = mgr.genericIntrinsics;
  auto &types = mgr.types;
  auto &valueEncoder = mgr.valueEncoder;
  auto &theModule = mgr.theModule;

  auto regUnaryMath = [&](const string &intrinsicName,
                          const string &wrapperName) {
    genericIntrinsics[intrinsicName] =
        [&mgr, wrapperName](auto &b, auto &args, CleanupChainGuard *guard) {
          ObjectTypeSet dT(doubleType);
          return mgr
              .invokeRuntime(wrapperName, &dT, {ObjectTypeSet::all()},
                             {args[0]}, false, guard)
              .value;
        };
  };

  auto regBinaryMath = [&](const string &intrinsicName,
                           const string &wrapperName) {
    genericIntrinsics[intrinsicName] =
        [&mgr, wrapperName](auto &b, auto &args, CleanupChainGuard *guard) {
          ObjectTypeSet dT(doubleType);
          return mgr
              .invokeRuntime(wrapperName, &dT,
                             {ObjectTypeSet::all(), ObjectTypeSet::all()},
                             {args[0], args[1]}, false, guard)
              .value;
        };
  };

  auto regUnarySpecialized = [&](const string &intrinsicName,
                                 const string &stdName, bool fromInt) {
    intrinsics[intrinsicName] = [&mgr, &types, stdName,
                                 fromInt](llvm::IRBuilder<> &b,
                                          std::vector<Value *> args) {
      Value *val = args[0];
      if (fromInt) {
        val = b.CreateSIToFP(val, types.doubleTy, "conv_d");
      }
      FunctionType *ft =
          FunctionType::get(types.doubleTy, {types.doubleTy}, false);
      FunctionCallee fn = mgr.theModule.getOrInsertFunction(stdName, ft);
      return b.CreateCall(fn, {val});
    };
  };

  auto regBinarySpecialized = [&](const string &intrinsicName,
                                  const string &stdName, bool fromIntLeft,
                                  bool fromIntRight) {
    intrinsics[intrinsicName] = [&mgr, &types, stdName, fromIntLeft,
                                 fromIntRight](llvm::IRBuilder<> &b,
                                               std::vector<Value *> args) {
      Value *val1 = args[0];
      Value *val2 = args[1];
      if (fromIntLeft) {
        val1 = b.CreateSIToFP(val1, types.doubleTy, "conv_d");
      }
      if (fromIntRight) {
        val2 = b.CreateSIToFP(val2, types.doubleTy, "conv_d");
      }
      FunctionType *ft = FunctionType::get(
          types.doubleTy, {types.doubleTy, types.doubleTy}, false);
      FunctionCallee fn = mgr.theModule.getOrInsertFunction(stdName, ft);
      return b.CreateCall(fn, {val1, val2});
    };
  };

  auto regMath = [&](const string &name, const string &stdName,
                     bool binary = false) {
    if (binary) {
      regBinaryMath("Math_" + name, "Numbers_" + name);
      regBinarySpecialized("Math_" + name + "_DD", stdName, false, false);
      regBinarySpecialized("Math_" + name + "_DI", stdName, false, true);
      regBinarySpecialized("Math_" + name + "_ID", stdName, true, false);
      regBinarySpecialized("Math_" + name + "_II", stdName, true, true);
    } else {
      regUnaryMath("Math_" + name, "Numbers_" + name);
      regUnarySpecialized("Math_" + name + "_D", stdName, false);
      regUnarySpecialized("Math_" + name + "_I", stdName, true);
    }
  };

  regMath("sin", "sin");
  regMath("cos", "cos");
  regMath("tan", "tan");
  regMath("asin", "asin");
  regMath("acos", "acos");
  regMath("atan", "atan");
  regMath("exp", "exp");
  regMath("exp2", "exp2");
  regMath("exp10", "exp10");
  regMath("log", "log");
  regMath("log10", "log10");
  regMath("logb", "logb");
  regMath("log2", "log2");
  regMath("sqrt", "sqrt");
  regMath("cbrt", "cbrt");
  regMath("exp1m", "exp1m");
  regMath("log1p", "log1p");
  regMath("abs", "fabs");

  regMath("atan2", "atan2", true);
  regMath("pow", "pow", true);
  regMath("hypot", "hypot", true);

  // --- Folding ---

  typeIntrinsics["Add"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2)
      return ObjectTypeSet(integerType, false,
                           new ConstantInteger(i1->value + i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Sub"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2)
      return ObjectTypeSet(integerType, false,
                           new ConstantInteger(i1->value - i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Mul"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2)
      return ObjectTypeSet(integerType, false,
                           new ConstantInteger(i1->value * i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Div"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2 && i2->value != 0)
      return ObjectTypeSet(integerType, false,
                           new ConstantInteger(i1->value / i2->value));
    return ObjectTypeSet(integerType, false);
  };

  typeIntrinsics["FAdd"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2)
      return ObjectTypeSet(doubleType, false,
                           new ConstantDouble(d1->value + d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FSub"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2)
      return ObjectTypeSet(doubleType, false,
                           new ConstantDouble(d1->value - d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FMul"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2)
      return ObjectTypeSet(doubleType, false,
                           new ConstantDouble(d1->value * d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FDiv"] =
      [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2 && d2->value != 0.0)
      return ObjectTypeSet(doubleType, false,
                           new ConstantDouble(d1->value / d2->value));
    return ObjectTypeSet(doubleType, false);
  };

  // BigInt pure math folding
  auto regZMath = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] =
        [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2)
        return ObjectTypeSet(bigIntegerType, false);
      mpz_ptr p1 = mgr.getZ(args[0]);
      mpz_ptr p2 = mgr.getZ(args[1]);
      if (p1 && p2) {
        mpz_t res;
        mpz_init(res);
        op(res, p1, p2);
        auto ret = mgr.createZ(res);
        mpz_clear(p1);
        mpz_clear(p2);
        mpz_clear(res);
        free(p1);
        free(p2);
        return ret;
      }
      if (p1) {
        mpz_clear(p1);
        free(p1);
      }
      if (p2) {
        mpz_clear(p2);
        free(p2);
      }
      return ObjectTypeSet(bigIntegerType, false);
    };
  };
  regZMath("BigInteger_add", mpz_add);
  regZMath("BigInteger_sub", mpz_sub);
  regZMath("BigInteger_mul", mpz_mul);

  typeIntrinsics["BigInteger_div"] =
      [&mgr](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2)
      return ObjectTypeSet::dynamicType();
    mpq_ptr p1 = mgr.getQ(args[0]);
    mpq_ptr p2 = mgr.getQ(args[1]);
    if (p1 && p2) {
      if (mpq_sgn(p2) == 0) {
        mpq_clear(p1);
        mpq_clear(p2);
        free(p1);
        free(p2);
        return ObjectTypeSet::dynamicType();
      }
      mpq_t res;
      mpq_init(res);
      mpq_div(res, p1, p2);
      auto ret = mgr.createQ(res);
      mpq_clear(p1);
      mpq_clear(p2);
      mpq_clear(res);
      free(p1);
      free(p2);
      return ret;
    }
    if (p1) {
      mpq_clear(p1);
      free(p1);
    }
    if (p2) {
      mpq_clear(p2);
      free(p2);
    }
    return ObjectTypeSet::dynamicType();
  };
  typeIntrinsics["Integer_div"] = typeIntrinsics["BigInteger_div"];

  // Ratio pure math folding
  auto regQMath = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] =
        [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2)
        return ObjectTypeSet(ratioType, false);
      mpq_ptr p1 = mgr.getQ(args[0]);
      mpq_ptr p2 = mgr.getQ(args[1]);
      if (p1 && p2) {
        mpq_t res;
        mpq_init(res);
        op(res, p1, p2);
        auto ret = mgr.createQ(res);
        mpq_clear(p1);
        mpq_clear(p2);
        mpq_clear(res);
        free(p1);
        free(p2);
        return ret;
      }
      if (p1) {
        mpq_clear(p1);
        free(p1);
      }
      if (p2) {
        mpq_clear(p2);
        free(p2);
      }
      return ObjectTypeSet(ratioType, false);
    };
  };
  regQMath("Ratio_add", mpq_add);
  regQMath("Ratio_sub", mpq_sub);
  regQMath("Ratio_mul", mpq_mul);
  regQMath("Ratio_div", [](mpq_t r, mpq_t a, mpq_t b) {
    if (mpq_sgn(b) != 0)
      mpq_div(r, a, b);
  });

  // Mixed type folding helpers
  auto regMixZ = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] =
        [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2)
        return ObjectTypeSet(bigIntegerType, false);
      mpz_ptr p1 = mgr.getZ(args[0]);
      mpz_ptr p2 = mgr.getZ(args[1]);
      if (p1 && p2) {
        mpz_t res;
        mpz_init(res);
        op(res, p1, p2);
        auto ret = mgr.createZ(res);
        mpz_clear(p1);
        mpz_clear(p2);
        mpz_clear(res);
        free(p1);
        free(p2);
        return ret;
      }
      if (p1) {
        mpz_clear(p1);
        free(p1);
      }
      if (p2) {
        mpz_clear(p2);
        free(p2);
      }
      return ObjectTypeSet(bigIntegerType, false);
    };
  };
  regMixZ("Add_IB", mpz_add);
  regMixZ("Add_BI", mpz_add);
  regMixZ("Sub_IB", mpz_sub);
  regMixZ("Sub_BI", mpz_sub);
  regMixZ("Mul_IB", mpz_mul);
  regMixZ("Mul_BI", mpz_mul);
  regMixZ("Div_IB", [](mpz_t r, mpz_t a, mpz_t b) {
    if (mpz_sgn(b) != 0)
      mpz_tdiv_q(r, a, b);
  });
  regMixZ("Div_BI", [](mpz_t r, mpz_t a, mpz_t b) {
    if (mpz_sgn(b) != 0)
      mpz_tdiv_q(r, a, b);
  });

  auto regMixD = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] =
        [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2)
        return ObjectTypeSet(doubleType, false);
      double d1 = mgr.getD(args[0]);
      double d2 = mgr.getD(args[1]);
      return ObjectTypeSet(doubleType, false, new ConstantDouble(op(d1, d2)));
    };
  };
  regMixD("Add_ID", [](double a, double b) { return a + b; });
  regMixD("Add_DI", [](double a, double b) { return a + b; });
  regMixD("Sub_ID", [](double a, double b) { return a - b; });
  regMixD("Sub_DI", [](double a, double b) { return a - b; });
  regMixD("Mul_ID", [](double a, double b) { return a * b; });
  regMixD("Mul_DI", [](double a, double b) { return a * b; });
  regMixD("Div_ID", [](double a, double b) { return a / b; });
  regMixD("Div_DI", [](double a, double b) { return a / b; });
  regMixD("Add_BD", [](double a, double b) { return a + b; });
  regMixD("Add_DB", [](double a, double b) { return a + b; });
  regMixD("Sub_BD", [](double a, double b) { return a - b; });
  regMixD("Sub_DB", [](double a, double b) { return a - b; });
  regMixD("Mul_BD", [](double a, double b) { return a * b; });
  regMixD("Mul_DB", [](double a, double b) { return a * b; });
  regMixD("Div_BD", [](double a, double b) { return a / b; });
  regMixD("Div_DB", [](double a, double b) { return a / b; });
  regMixD("Add_RD", [](double a, double b) { return a + b; });
  regMixD("Add_DR", [](double a, double b) { return a + b; });
  regMixD("Sub_RD", [](double a, double b) { return a - b; });
  regMixD("Sub_DR", [](double a, double b) { return a - b; });
  regMixD("Mul_RD", [](double a, double b) { return a * b; });
  regMixD("Mul_DR", [](double a, double b) { return a * b; });
  regMixD("Div_RD", [](double a, double b) { return a / b; });
  regMixD("Div_DR", [](double a, double b) { return a / b; });

  auto regMixQ = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] =
        [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2)
        return ObjectTypeSet(ratioType, false);
      mpq_ptr p1 = mgr.getQ(args[0]);
      mpq_ptr p2 = mgr.getQ(args[1]);
      if (p1 && p2) {
        mpq_t res;
        mpq_init(res);
        op(res, p1, p2);
        auto ret = mgr.createQ(res);
        mpq_clear(p1);
        mpq_clear(p2);
        mpq_clear(res);
        free(p1);
        free(p2);
        return ret;
      }
      if (p1) {
        mpq_clear(p1);
        free(p1);
      }
      if (p2) {
        mpq_clear(p2);
        free(p2);
      }
      return ObjectTypeSet(ratioType, false);
    };
  };
  regMixQ("Add_BR", mpq_add);
  regMixQ("Add_RB", mpq_add);
  regMixQ("Sub_BR", mpq_sub);
  regMixQ("Sub_RB", mpq_sub);
  regMixQ("Mul_BR", mpq_mul);
  regMixQ("Mul_RB", mpq_mul);
  regMixQ("Div_BR", [](mpq_t r, mpq_t a, mpq_t b) {
    if (mpq_sgn(b) != 0)
      mpq_div(r, a, b);
  });
  regMixQ("Div_RB", [](mpq_t r, mpq_t a, mpq_t b) {
    if (mpq_sgn(b) != 0)
      mpq_div(r, a, b);
  });
  regMixQ("Add_IR", mpq_add);
  regMixQ("Add_RI", mpq_add);
  regMixQ("Sub_IR", mpq_sub);
  regMixQ("Sub_RI", mpq_sub);
  regMixQ("Mul_IR", mpq_mul);
  regMixQ("Mul_RI", mpq_mul);
  regMixQ("Div_IR", [](mpq_t r, mpq_t a, mpq_t b) {
    if (mpq_sgn(b) != 0)
      mpq_div(r, a, b);
  });
  regMixQ("Div_RI", [](mpq_t r, mpq_t a, mpq_t b) {
    if (mpq_sgn(b) != 0)
      mpq_div(r, a, b);
  });

  // --- Codegen ---

  intrinsics["FAdd"] = [](auto &b, auto args) {
    return b.CreateFAdd(args[0], args[1]);
  };

  auto createCheckedOp = [&mgr, &theModule,
                          &types](const std::string &llvmIntrinsic,
                                  const std::string &errorMessage) {
    return [&mgr, &theModule, &types, llvmIntrinsic, errorMessage](
               llvm::IRBuilder<> &b, std::vector<llvm::Value *> args) {
      Function *fn = Intrinsic::getOrInsertDeclaration(
          &theModule,
          llvmIntrinsic == "sadd" ? Intrinsic::sadd_with_overflow
                                  : Intrinsic::ssub_with_overflow,
          {b.getInt32Ty()});
      Value *resStruct = b.CreateCall(fn, args);
      Value *res = b.CreateExtractValue(resStruct, {0});
      Value *overflow = b.CreateExtractValue(resStruct, {1});

      BasicBlock *insertBB = b.GetInsertBlock();
      Function *currentFn = insertBB->getParent();

      BasicBlock *overflowBB =
          BasicBlock::Create(theModule.getContext(), "overflow", currentFn);
      BasicBlock *continueBB =
          BasicBlock::Create(theModule.getContext(), "no_overflow", currentFn);

      MDBuilder MDB(theModule.getContext());
      MDNode *Weights = MDB.createBranchWeights(1, 1000000);
      b.CreateCondBr(overflow, overflowBB, continueBB, Weights);

      b.SetInsertPoint(overflowBB);
      FunctionType *throwFnTy =
          FunctionType::get(types.voidTy, {mgr.types.ptrTy}, false);
      FunctionCallee throwFn = theModule.getOrInsertFunction(
          "throwArithmeticException_C", throwFnTy);
      Value *msg = b.CreateGlobalString(errorMessage);
      b.CreateCall(throwFn, {msg});
      b.CreateUnreachable();

      b.SetInsertPoint(continueBB);
      return res;
    };
  };

  intrinsics["Add"] = createCheckedOp("sadd", "Integer overflow");
  intrinsics["Sub"] = createCheckedOp("ssub", "Integer overflow");
  intrinsics["FSub"] = [](auto &b, auto args) {
    return b.CreateFSub(args[0], args[1]);
  };
  intrinsics["FMul"] = [](auto &b, auto args) {
    return b.CreateFMul(args[0], args[1]);
  };
  intrinsics["Mul"] = [](auto &b, auto args) {
    return b.CreateMul(args[0], args[1]);
  };
  intrinsics["FDiv"] = [](auto &b, auto args) {
    return b.CreateFDiv(args[0], args[1]);
  };
  intrinsics["Div"] = [](auto &b, auto args) {
    return b.CreateSDiv(args[0], args[1]);
  };

  // BigInt/Ratio Codegen
  auto regZCodegen = [&mgr, &intrinsics](const string &symbol,
                                         const string &fnName) {
    intrinsics[symbol] = [&mgr, fnName](auto &b, auto args) {
      ObjectTypeSet z(bigIntegerType);
      return mgr
          .invokeRuntime(fnName, &z, {z, z},
                         {TypedValue(z, args[0]), TypedValue(z, args[1])})
          .value;
    };
  };
  regZCodegen("BigInteger_add", "BigInteger_add");
  regZCodegen("BigInteger_sub", "BigInteger_sub");
  regZCodegen("BigInteger_mul", "BigInteger_mul");
  intrinsics["BigInteger_div"] = [&mgr](auto &b, auto args) {
    ObjectTypeSet z(bigIntegerType);
    ObjectTypeSet anyT = ObjectTypeSet::all();
    return mgr
        .invokeRuntime("BigInteger_div", &anyT, {z, z},
                       {TypedValue(z, args[0]), TypedValue(z, args[1])})
        .value;
  };
  intrinsics["Integer_div"] = [&mgr](auto &b, auto args) {
    ObjectTypeSet i(integerType);
    ObjectTypeSet anyT = ObjectTypeSet::all();
    return mgr
        .invokeRuntime("Integer_div", &anyT, {i, i},
                       {TypedValue(i, args[0]), TypedValue(i, args[1])})
        .value;
  };

  auto regQCodegen = [&mgr, &intrinsics](const string &symbol,
                                         const string &fnName) {
    intrinsics[symbol] = [&mgr, fnName](auto &b, auto args) {
      ObjectTypeSet q(ratioType);
      ObjectTypeSet anyT = ObjectTypeSet::all();
      return mgr
          .invokeRuntime(fnName, &anyT, {q, q},
                         {TypedValue(q, args[0]), TypedValue(q, args[1])})
          .value;
    };
  };
  regQCodegen("Ratio_add", "Ratio_add");
  regQCodegen("Ratio_sub", "Ratio_sub");
  regQCodegen("Ratio_mul", "Ratio_mul");
  regQCodegen("Ratio_div", "Ratio_div");

  // Mixed-type Arithmetic Codegen
  intrinsics["Add_ID"] = [&mgr](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], mgr.types.doubleTy, "conv");
    return b.CreateFAdd(v1, args[1]);
  };
  intrinsics["Add_DI"] = [&mgr](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], mgr.types.doubleTy, "conv");
    return b.CreateFAdd(args[0], v2);
  };
  intrinsics["Sub_ID"] = [&mgr](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], mgr.types.doubleTy, "conv");
    return b.CreateFSub(v1, args[1]);
  };
  intrinsics["Sub_DI"] = [&mgr](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], mgr.types.doubleTy, "conv");
    return b.CreateFSub(args[0], v2);
  };
  intrinsics["Mul_ID"] = [&mgr](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], mgr.types.doubleTy, "conv");
    return b.CreateFMul(v1, args[1]);
  };
  intrinsics["Mul_DI"] = [&mgr](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], mgr.types.doubleTy, "conv");
    return b.CreateFMul(args[0], v2);
  };
  intrinsics["Div_ID"] = [&mgr](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], mgr.types.doubleTy, "conv");
    return b.CreateFDiv(v1, args[1]);
  };
  intrinsics["Div_DI"] = [&mgr](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], mgr.types.doubleTy, "conv");
    return b.CreateFDiv(args[0], v2);
  };

  auto makeIB = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      auto retType = ObjectTypeSet(bigIntegerType);
      TypedValue v1 = mgr.invokeRuntime(
          "BigInteger_createFromInt", &retType, {ObjectTypeSet(integerType)},
          {TypedValue(ObjectTypeSet(integerType), args[0])});
      return mgr
          .invokeRuntime(
              opSymbol, &retType,
              {ObjectTypeSet(bigIntegerType), ObjectTypeSet(bigIntegerType)},
              {v1, TypedValue(ObjectTypeSet(bigIntegerType), args[1])})
          .value;
    };
  };

  auto makeBI = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      auto retType = ObjectTypeSet(bigIntegerType);
      TypedValue v2 = mgr.invokeRuntime(
          "BigInteger_createFromInt", &retType, {ObjectTypeSet(integerType)},
          {TypedValue(ObjectTypeSet(integerType), args[1])});
      return mgr
          .invokeRuntime(
              opSymbol, &retType,
              {ObjectTypeSet(bigIntegerType), ObjectTypeSet(bigIntegerType)},
              {TypedValue(ObjectTypeSet(bigIntegerType), args[0]), v2})
          .value;
    };
  };

  intrinsics["Add_IB"] = makeIB("BigInteger_add");
  intrinsics["Add_BI"] = makeBI("BigInteger_add");
  intrinsics["Sub_IB"] = makeIB("BigInteger_sub");
  intrinsics["Sub_BI"] = makeBI("BigInteger_sub");
  intrinsics["Mul_IB"] = makeIB("BigInteger_mul");
  intrinsics["Mul_BI"] = makeBI("BigInteger_mul");
  intrinsics["Div_IB"] = makeIB("BigInteger_div");
  intrinsics["Div_BI"] = makeBI("BigInteger_div");

  auto makeDB = [&mgr, &intrinsics](const std::string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 = mgr.invokeRuntime(
          "BigInteger_toDouble", &doubleTypeSet,
          {ObjectTypeSet(bigIntegerType)},
          {TypedValue(ObjectTypeSet(bigIntegerType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeBD = [&mgr, &intrinsics](const std::string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 = mgr.invokeRuntime(
          "BigInteger_toDouble", &doubleTypeSet,
          {ObjectTypeSet(bigIntegerType)},
          {TypedValue(ObjectTypeSet(bigIntegerType), args[0])});
      return intrinsics[opSymbol](b, {v1.value, args[1]});
    };
  };

  intrinsics["Add_BD"] = makeBD("FAdd");
  intrinsics["Add_DB"] = makeDB("FAdd");
  intrinsics["Sub_BD"] = makeBD("FSub");
  intrinsics["Sub_DB"] = makeDB("FSub");
  intrinsics["Mul_BD"] = makeBD("FMul");
  intrinsics["Mul_DB"] = makeBD("FMul");
  intrinsics["Div_BD"] = makeBD("FDiv");
  intrinsics["Div_DB"] = makeDB("FDiv");

  auto makeDR = [&mgr, &intrinsics](const std::string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 = mgr.invokeRuntime(
          "Ratio_toDouble", &doubleTypeSet, {ObjectTypeSet(ratioType)},
          {TypedValue(ObjectTypeSet(ratioType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeRD = [&mgr, &intrinsics](const std::string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 = mgr.invokeRuntime(
          "Ratio_toDouble", &doubleTypeSet, {ObjectTypeSet(ratioType)},
          {TypedValue(ObjectTypeSet(ratioType), args[0])});
      return intrinsics[opSymbol](b, {v1.value, args[1]});
    };
  };
  intrinsics["Add_RD"] = makeRD("FAdd");
  intrinsics["Add_DR"] = makeDR("FAdd");
  intrinsics["Sub_RD"] = makeRD("FSub");
  intrinsics["Sub_DR"] = makeDR("FSub");
  intrinsics["Mul_RD"] = makeRD("FMul");
  intrinsics["Mul_DR"] = makeDR("FMul");
  intrinsics["Div_RD"] = makeRD("FDiv");
  intrinsics["Div_DR"] = makeDR("FDiv");

  auto makeRB = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet bigIntTypeSet(bigIntegerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v2 = mgr.invokeRuntime("Ratio_createFromBigInteger", &retType,
                                        {bigIntTypeSet},
                                        {TypedValue(bigIntTypeSet, args[1])});
      return mgr
          .invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                         {TypedValue(retType, args[0]), v2})
          .value;
    };
  };

  auto makeBR = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet bigIntTypeSet(bigIntegerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v1 = mgr.invokeRuntime("Ratio_createFromBigInteger", &retType,
                                        {bigIntTypeSet},
                                        {TypedValue(bigIntTypeSet, args[0])});
      return mgr
          .invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                         {v1, TypedValue(retType, args[1])})
          .value;
    };
  };
  intrinsics["Add_BR"] = makeBR("Ratio_add");
  intrinsics["Add_RB"] = makeRB("Ratio_add");
  intrinsics["Sub_BR"] = makeBR("Ratio_sub");
  intrinsics["Sub_RB"] = makeRB("Ratio_sub");
  intrinsics["Mul_BR"] = makeBR("Ratio_mul");
  intrinsics["Mul_RB"] = makeRB("Ratio_mul");
  intrinsics["Div_BR"] = makeBR("Ratio_div");
  intrinsics["Div_RB"] = makeRB("Ratio_div");

  auto makeRI = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet intTypeSet(integerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v2 =
          mgr.invokeRuntime("Ratio_createFromInt", &retType, {intTypeSet},
                            {TypedValue(intTypeSet, args[1])});
      return mgr
          .invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                         {TypedValue(retType, args[0]), v2})
          .value;
    };
  };

  auto makeIR = [&mgr](const std::string &opSymbol) {
    return [&mgr, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet intTypeSet(integerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v1 =
          mgr.invokeRuntime("Ratio_createFromInt", &retType, {intTypeSet},
                            {TypedValue(intTypeSet, args[0])});
      return mgr
          .invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                         {v1, TypedValue(retType, args[1])})
          .value;
    };
  };
  intrinsics["Add_IR"] = makeIR("Ratio_add");
  intrinsics["Add_RI"] = makeRI("Ratio_add");
  intrinsics["Sub_IR"] = makeIR("Ratio_sub");
  intrinsics["Sub_RI"] = makeRI("Ratio_sub");
  intrinsics["Mul_IR"] = makeIR("Ratio_mul");
  intrinsics["Mul_RI"] = makeRI("Ratio_mul");
  intrinsics["Div_IR"] = makeIR("Ratio_div");
  intrinsics["Div_RI"] = makeRI("Ratio_div");
}

} // namespace rt
