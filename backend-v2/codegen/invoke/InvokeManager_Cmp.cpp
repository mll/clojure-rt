#include "InvokeManager.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
#include "../../types/ConstantBigInteger.h"
#include "../../types/ConstantRatio.h"

using namespace llvm;
using namespace std;

namespace rt {

void registerCmpIntrinsics(InvokeManager &mgr) {
  auto &typeIntrinsics = mgr.typeIntrinsics;
  auto &intrinsics = mgr.intrinsics;

  // --- Folding ---

  typeIntrinsics["ICmpSGE"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(i1->value >= i2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["ICmpSGT"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(i1->value > i2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["ICmpSLT"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(i1->value < i2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["ICmpSLE"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(i1->value <= i2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["ICmpEQ"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(i1->value == i2->value));
    return ObjectTypeSet(booleanType, false);
  };

  typeIntrinsics["FCmpOGE"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(d1->value >= d2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["FCmpOGT"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(d1->value > d2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["FCmpOLT"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(d1->value < d2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["FCmpOLE"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(d1->value <= d2->value));
    return ObjectTypeSet(booleanType, false);
  };
  typeIntrinsics["FCmpOEQ"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(booleanType, false, new ConstantBoolean(d1->value == d2->value));
    return ObjectTypeSet(booleanType, false);
  };

  // BigInt Comparisons Folding
  auto regZCmp = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] = [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2) return ObjectTypeSet(booleanType, false);
      mpz_ptr p1 = mgr.getZ(args[0]);
      mpz_ptr p2 = mgr.getZ(args[1]);
      if (p1 && p2) {
        bool res = op(p1, p2);
        mpz_clear(p1); mpz_clear(p2);
        free(p1); free(p2);
        return ObjectTypeSet(booleanType, false, new ConstantBoolean(res));
      }
      if (p1) { mpz_clear(p1); free(p1); }
      if (p2) { mpz_clear(p2); free(p2); }
      return ObjectTypeSet(booleanType, false);
    };
  };
  regZCmp("BigInteger_gte", [](mpz_t a, mpz_t b) { return mpz_cmp(a, b) >= 0; });
  regZCmp("BigInteger_gt", [](mpz_t a, mpz_t b) { return mpz_cmp(a, b) > 0; });
  regZCmp("BigInteger_lt", [](mpz_t a, mpz_t b) { return mpz_cmp(a, b) < 0; });
  regZCmp("BigInteger_lte", [](mpz_t a, mpz_t b) { return mpz_cmp(a, b) <= 0; });
  regZCmp("BigInteger_equiv", [](mpz_t a, mpz_t b) { return mpz_cmp(a, b) == 0; });

  // Ratio Comparisons Folding
  auto regQCmp = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] = [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2) return ObjectTypeSet(booleanType, false);
      mpq_ptr p1 = mgr.getQ(args[0]);
      mpq_ptr p2 = mgr.getQ(args[1]);
      if (p1 && p2) {
        bool res = op(p1, p2);
        mpq_clear(p1); mpq_clear(p2);
        free(p1); free(p2);
        return ObjectTypeSet(booleanType, false, new ConstantBoolean(res));
      }
      if (p1) { mpq_clear(p1); free(p1); }
      if (p2) { mpq_clear(p2); free(p2); }
      return ObjectTypeSet(booleanType, false);
    };
  };
  regQCmp("Ratio_gte", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) >= 0; });
  regQCmp("Ratio_gt", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) > 0; });
  regQCmp("Ratio_lt", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) < 0; });
  regQCmp("Ratio_lte", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) <= 0; });
  regQCmp("Ratio_equiv", [](mpq_t a, mpq_t b) { return mpq_equal(a, b); });

  // Mixed-type Comparison Folding
  auto regMixedDCmp = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] = [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2) return ObjectTypeSet(booleanType, false);
      double d1 = mgr.getD(args[0]);
      double d2 = mgr.getD(args[1]);
      return ObjectTypeSet(booleanType, false, new ConstantBoolean(op(d1, d2)));
    };
  };
  regMixedDCmp("ICmpSGE_ID", [](double a, double b) { return a >= b; });
  regMixedDCmp("ICmpSGE_DI", [](double a, double b) { return a >= b; });
  regMixedDCmp("ICmpSGT_ID", [](double a, double b) { return a > b; });
  regMixedDCmp("ICmpSGT_DI", [](double a, double b) { return a > b; });
  regMixedDCmp("ICmpSLT_ID", [](double a, double b) { return a < b; });
  regMixedDCmp("ICmpSLT_DI", [](double a, double b) { return a < b; });
  regMixedDCmp("ICmpSLE_ID", [](double a, double b) { return a <= b; });
  regMixedDCmp("ICmpSLE_DI", [](double a, double b) { return a <= b; });
  regMixedDCmp("FCmpOEQ_ID", [](double a, double b) { return a == b; });
  regMixedDCmp("FCmpOEQ_DI", [](double a, double b) { return a == b; });
  regMixedDCmp("FCmpOGE_BD", [](double a, double b) { return a >= b; });
  regMixedDCmp("FCmpOGE_DB", [](double a, double b) { return a >= b; });
  regMixedDCmp("FCmpOGT_BD", [](double a, double b) { return a > b; });
  regMixedDCmp("FCmpOGT_DB", [](double a, double b) { return a > b; });
  regMixedDCmp("FCmpOLT_BD", [](double a, double b) { return a < b; });
  regMixedDCmp("FCmpOLT_DB", [](double a, double b) { return a < b; });
  regMixedDCmp("FCmpOLE_BD", [](double a, double b) { return a <= b; });
  regMixedDCmp("FCmpOLE_DB", [](double a, double b) { return a <= b; });
  regMixedDCmp("FCmpOEQ_BD", [](double a, double b) { return a == b; });
  regMixedDCmp("FCmpOEQ_DB", [](double a, double b) { return a == b; });
  regMixedDCmp("FCmpOGE_RD", [](double a, double b) { return a >= b; });
  regMixedDCmp("FCmpOGE_DR", [](double a, double b) { return a >= b; });
  regMixedDCmp("FCmpOGT_RD", [](double a, double b) { return a > b; });
  regMixedDCmp("FCmpOGT_DR", [](double a, double b) { return a > b; });
  regMixedDCmp("FCmpOLT_RD", [](double a, double b) { return a < b; });
  regMixedDCmp("FCmpOLT_DR", [](double a, double b) { return a < b; });
  regMixedDCmp("FCmpOLE_RD", [](double a, double b) { return a <= b; });
  regMixedDCmp("FCmpOLE_DR", [](double a, double b) { return a <= b; });
  regMixedDCmp("FCmpOEQ_RD", [](double a, double b) { return a == b; });
  regMixedDCmp("FCmpOEQ_DR", [](double a, double b) { return a == b; });

  auto regMixedQCmp = [&](const string &symbol, auto op) {
    typeIntrinsics[symbol] = [&mgr, op](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
      if (args.size() != 2) return ObjectTypeSet(booleanType, false);
      mpq_ptr p1 = mgr.getQ(args[0]);
      mpq_ptr p2 = mgr.getQ(args[1]);
      if (p1 && p2) {
        bool res = op(p1, p2);
        mpq_clear(p1); mpq_clear(p2);
        free(p1); free(p2);
        return ObjectTypeSet(booleanType, false, new ConstantBoolean(res));
      }
      if (p1) { mpq_clear(p1); free(p1); }
      if (p2) { mpq_clear(p2); free(p2); }
      return ObjectTypeSet(booleanType, false);
    };
  };
  regMixedQCmp("FCmpOGE_BR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) >= 0; });
  regMixedQCmp("FCmpOGE_RB", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) >= 0; });
  regMixedQCmp("FCmpOGT_BR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) > 0; });
  regMixedQCmp("FCmpOGT_RB", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) > 0; });
  regMixedQCmp("FCmpOLT_BR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) < 0; });
  regMixedQCmp("FCmpOLT_RB", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) < 0; });
  regMixedQCmp("FCmpOLE_BR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) <= 0; });
  regMixedQCmp("FCmpOLE_RB", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) <= 0; });
  regMixedQCmp("FCmpOGE_IR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) >= 0; });
  regMixedQCmp("FCmpOGE_RI", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) >= 0; });
  regMixedQCmp("FCmpOGT_IR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) > 0; });
  regMixedQCmp("FCmpOGT_RI", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) > 0; });
  regMixedQCmp("FCmpOLT_IR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) < 0; });
  regMixedQCmp("FCmpOLT_RI", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) < 0; });
  regMixedQCmp("FCmpOLE_IR", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) <= 0; });
  regMixedQCmp("FCmpOLE_RI", [](mpq_t a, mpq_t b) { return mpq_cmp(a, b) <= 0; });


  // --- Codegen ---

  intrinsics["FCmpOGE"] = [](auto &b, auto args) {
    return b.CreateFCmpOGE(args[0], args[1]);
  };
  intrinsics["ICmpSGE"] = [](auto &b, auto args) {
    return b.CreateICmpSGE(args[0], args[1]);
  };
  intrinsics["FCmpOGT"] = [](auto &b, auto args) {
    return b.CreateFCmpOGT(args[0], args[1]);
  };
  intrinsics["ICmpSGT"] = [](auto &b, auto args) {
    return b.CreateICmpSGT(args[0], args[1]);
  };
  intrinsics["FCmpOLT"] = [](auto &b, auto args) {
    return b.CreateFCmpOLT(args[0], args[1]);
  };
  intrinsics["FCmpOLE"] = [](auto &b, auto args) {
    return b.CreateFCmpOLE(args[0], args[1]);
  };
  intrinsics["ICmpSLT"] = [](auto &b, auto args) {
    return b.CreateICmpSLT(args[0], args[1]);
  };
  intrinsics["ICmpSLE"] = [](auto &b, auto args) {
    return b.CreateICmpSLE(args[0], args[1]);
  };
  intrinsics["ICmpEQ"] = [](auto &b, auto args) {
    return b.CreateICmpEQ(args[0], args[1]);
  };
  intrinsics["FCmpOEQ"] = [](auto &b, auto args) {
    return b.CreateFCmpOEQ(args[0], args[1]);
  };

  // BigInt/Ratio Comparisons Codegen
  auto regZCmpCodegen = [&mgr, &intrinsics](const string &symbol, const string &fnName) {
    intrinsics[symbol] = [&mgr, fnName](auto &b, auto args) {
      ObjectTypeSet z(bigIntegerType);
      ObjectTypeSet boolT(booleanType);
      return mgr.invokeRuntime(fnName, &boolT, {z, z}, {TypedValue(z, args[0]), TypedValue(z, args[1])}).value;
    };
  };
  regZCmpCodegen("BigInteger_gte", "BigInteger_gte");
  regZCmpCodegen("BigInteger_gt", "BigInteger_gt");
  regZCmpCodegen("BigInteger_lt", "BigInteger_lt");
  regZCmpCodegen("BigInteger_lte", "BigInteger_lte");
  regZCmpCodegen("BigInteger_equiv", "BigInteger_equiv");

  auto regQCmpCodegen = [&mgr, &intrinsics](const string &symbol, const string &fnName) {
    intrinsics[symbol] = [&mgr, fnName](auto &b, auto args) {
      ObjectTypeSet q(ratioType);
      ObjectTypeSet boolT(booleanType);
      return mgr.invokeRuntime(fnName, &boolT, {q, q}, {TypedValue(q, args[0]), TypedValue(q, args[1])}).value;
    };
  };
  regQCmpCodegen("Ratio_gte", "Ratio_gte");
  regQCmpCodegen("Ratio_gt", "Ratio_gt");
  regQCmpCodegen("Ratio_lt", "Ratio_lt");
  regQCmpCodegen("Ratio_lte", "Ratio_lte");
  regQCmpCodegen("Ratio_equiv", "Ratio_equiv");

  // Mixed BigInt/Double Comparison Codegen
  auto makeDB = [&mgr, &intrinsics](const string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 = mgr.invokeRuntime("BigInteger_toDouble", &doubleTypeSet,
                         {ObjectTypeSet(bigIntegerType)},
                         {TypedValue(ObjectTypeSet(bigIntegerType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeBD = [&mgr, &intrinsics](const string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 = mgr.invokeRuntime("BigInteger_toDouble", &doubleTypeSet,
                         {ObjectTypeSet(bigIntegerType)},
                         {TypedValue(ObjectTypeSet(bigIntegerType), args[0])});
      return intrinsics[opSymbol](b, {v1.value, args[1]});
    };
  };

  intrinsics["FCmpOGE_BD"] = makeBD("FCmpOGE");
  intrinsics["FCmpOGE_DB"] = makeDB("FCmpOGE");
  intrinsics["FCmpOGT_BD"] = makeBD("FCmpOGT");
  intrinsics["FCmpOGT_DB"] = makeDB("FCmpOGT");
  intrinsics["FCmpOLT_BD"] = makeBD("FCmpOLT");
  intrinsics["FCmpOLT_DB"] = makeDB("FCmpOLT");
  intrinsics["FCmpOLE_BD"] = makeBD("FCmpOLE");
  intrinsics["FCmpOLE_DB"] = makeDB("FCmpOLE");
  intrinsics["FCmpOEQ_BD"] = makeBD("FCmpOEQ");
  intrinsics["FCmpOEQ_DB"] = makeDB("FCmpOEQ");

  auto makeDR = [&mgr, &intrinsics](const string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 = mgr.invokeRuntime("Ratio_toDouble", &doubleTypeSet,
                         {ObjectTypeSet(ratioType)},
                         {TypedValue(ObjectTypeSet(ratioType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeRD = [&mgr, &intrinsics](const string &opSymbol) {
    return [&mgr, &intrinsics, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 = mgr.invokeRuntime("Ratio_toDouble", &doubleTypeSet,
                         {ObjectTypeSet(ratioType)},
                         {TypedValue(ObjectTypeSet(ratioType), args[0])});
      return intrinsics[opSymbol](b, {v1.value, args[1]});
    };
  };

  intrinsics["FCmpOGE_RD"] = makeRD("FCmpOGE");
  intrinsics["FCmpOGE_DR"] = makeDR("FCmpOGE");
  intrinsics["FCmpOGT_RD"] = makeRD("FCmpOGT");
  intrinsics["FCmpOGT_DR"] = makeDR("FCmpOGT");
  intrinsics["FCmpOLT_RD"] = makeRD("FCmpOLT");
  intrinsics["FCmpOLT_DR"] = makeDR("FCmpOLT");
  intrinsics["FCmpOLE_RD"] = makeRD("FCmpOLE");
  intrinsics["FCmpOLE_DR"] = makeDR("FCmpOLE");
  intrinsics["FCmpOEQ_RD"] = makeRD("FCmpOEQ");
  intrinsics["FCmpOEQ_DR"] = makeDR("FCmpOEQ");
}

} // namespace rt
