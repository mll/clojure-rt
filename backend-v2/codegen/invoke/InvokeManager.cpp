#include "InvokeManager.h"
#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantDouble.h"
#include "../../types/ConstantInteger.h"
// Redundant include removed
#include "llvm/IR/MDBuilder.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <vector>

using namespace llvm;
using namespace std;

namespace rt {

InvokeManager::InvokeManager(llvm::IRBuilder<> &b, llvm::Module &m,
                             ValueEncoder &v, LLVMTypes &t,
                             ThreadsafeCompilerState &s)
    : builder(b), theModule(m), valueEncoder(v), types(t) {
  // Basic Arithmetic
  typeIntrinsics["Add"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(integerType, false, new ConstantInteger(i1->value + i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Sub"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(integerType, false, new ConstantInteger(i1->value - i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Mul"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2) return ObjectTypeSet(integerType, false, new ConstantInteger(i1->value * i2->value));
    return ObjectTypeSet(integerType, false);
  };
  typeIntrinsics["Div"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *i1 = dynamic_cast<ConstantInteger *>(c1);
    auto *i2 = dynamic_cast<ConstantInteger *>(c2);
    if (i1 && i2 && i2->value != 0) return ObjectTypeSet(integerType, false, new ConstantInteger(i1->value / i2->value));
    return ObjectTypeSet(integerType, false);
  };

  typeIntrinsics["FAdd"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(doubleType, false, new ConstantDouble(d1->value + d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FSub"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(doubleType, false, new ConstantDouble(d1->value - d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FMul"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2) return ObjectTypeSet(doubleType, false, new ConstantDouble(d1->value * d2->value));
    return ObjectTypeSet(doubleType, false);
  };
  typeIntrinsics["FDiv"] = [](const vector<ObjectTypeSet> &args) -> ObjectTypeSet {
    if (args.size() != 2) return ObjectTypeSet::dynamicType();
    auto *c1 = args[0].getConstant();
    auto *c2 = args[1].getConstant();
    auto *d1 = dynamic_cast<ConstantDouble *>(c1);
    auto *d2 = dynamic_cast<ConstantDouble *>(c2);
    if (d1 && d2 && d2->value != 0.0) return ObjectTypeSet(doubleType, false, new ConstantDouble(d1->value / d2->value));
    return ObjectTypeSet(doubleType, false);
  };

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

  intrinsics["FAdd"] = [](auto &b, auto args) {
    return b.CreateFAdd(args[0], args[1]);
  };
  auto createCheckedOp = [this](const std::string &llvmIntrinsic,
                               const std::string &errorMessage) {
    return [this, llvmIntrinsic, errorMessage](llvm::IRBuilder<> &b,
                                               std::vector<llvm::Value *> args) {
      Function *fn = Intrinsic::getDeclaration(&theModule, 
          llvmIntrinsic == "sadd" ? Intrinsic::sadd_with_overflow : Intrinsic::ssub_with_overflow, 
          {b.getInt32Ty()});
      Value *resStruct = b.CreateCall(fn, args);
      Value *res = b.CreateExtractValue(resStruct, {0});
      Value *overflow = b.CreateExtractValue(resStruct, {1});

      BasicBlock *insertBB = b.GetInsertBlock();
      Function *currentFn = insertBB->getParent();

      BasicBlock *overflowBB = BasicBlock::Create(theModule.getContext(), "overflow", currentFn);
      BasicBlock *continueBB = BasicBlock::Create(theModule.getContext(), "no_overflow", currentFn);

      // Branch weights: heavily bias towards no overflow (happy path)
      MDBuilder MDB(theModule.getContext());
      MDNode *Weights = MDB.createBranchWeights(1, 1000000); // 1 = overflow, 1000000 = no overflow
      b.CreateCondBr(overflow, overflowBB, continueBB, Weights);

      b.SetInsertPoint(overflowBB);
      
      // throwArithmeticException_C("Integer overflow")
      FunctionType *throwFnTy = FunctionType::get(types.voidTy, {types.ptrTy}, false);
      FunctionCallee throwFn = theModule.getOrInsertFunction("throwArithmeticException_C", throwFnTy);
      
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

  // Comparisons
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

  // Mixed-type Arithmetic (Composite Intrinsics)
  intrinsics["Add_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFAdd(v1, args[1]);
  };
  intrinsics["Add_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFAdd(args[0], v2);
  };
  intrinsics["Sub_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFSub(v1, args[1]);
  };
  intrinsics["Sub_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFSub(args[0], v2);
  };
  intrinsics["Mul_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFMul(v1, args[1]);
  };
  intrinsics["Mul_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFMul(args[0], v2);
  };
  intrinsics["Div_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFDiv(v1, args[1]);
  };
  intrinsics["Div_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFDiv(args[0], v2);
  };

  // BigInt mixed with Int Helpers
  auto makeIB = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      auto retType = ObjectTypeSet(bigIntegerType);
      TypedValue v1 = invokeRuntime(
          "BigInteger_createFromInt", &retType, {ObjectTypeSet(integerType)},
          {TypedValue(ObjectTypeSet(integerType), args[0])});
      return invokeRuntime(
                 opSymbol, &retType,
                 {ObjectTypeSet(bigIntegerType), ObjectTypeSet(bigIntegerType)},
                 {v1, TypedValue(ObjectTypeSet(bigIntegerType), args[1])})
          .value;
    };
  };

  auto makeBI = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      auto retType = ObjectTypeSet(bigIntegerType);
      TypedValue v2 = invokeRuntime(
          "BigInteger_createFromInt", &retType, {ObjectTypeSet(integerType)},
          {TypedValue(ObjectTypeSet(integerType), args[1])});
      return invokeRuntime(
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

  // BigInt mixed with Double Helpers
  auto makeDB = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 =
          invokeRuntime("BigInteger_toDouble", &doubleTypeSet,
                        {ObjectTypeSet(bigIntegerType)},
                        {TypedValue(ObjectTypeSet(bigIntegerType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeBD = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 =
          invokeRuntime("BigInteger_toDouble", &doubleTypeSet,
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
  intrinsics["Mul_DB"] = makeDB("FMul");
  intrinsics["Div_BD"] = makeBD("FDiv");
  intrinsics["Div_DB"] = makeDB("FDiv");

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

  // Ratio mixed with Double Helpers
  auto makeDR = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v2 = invokeRuntime(
          "Ratio_toDouble", &doubleTypeSet, {ObjectTypeSet(ratioType)},
          {TypedValue(ObjectTypeSet(ratioType), args[1])});
      return intrinsics[opSymbol](b, {args[0], v2.value});
    };
  };

  auto makeRD = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet doubleTypeSet(doubleType);
      TypedValue v1 = invokeRuntime(
          "Ratio_toDouble", &doubleTypeSet, {ObjectTypeSet(ratioType)},
          {TypedValue(ObjectTypeSet(ratioType), args[0])});
      return intrinsics[opSymbol](b, {v1.value, args[1]});
    };
  };

  // BigInt mixed with Ratio Helpers
  auto makeRB = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet bigIntTypeSet(bigIntegerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v2 =
          invokeRuntime("Ratio_createFromBigInteger", &retType, {bigIntTypeSet},
                        {TypedValue(bigIntTypeSet, args[1])});
      return invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                           {TypedValue(retType, args[0]), v2})
          .value;
    };
  };

  auto makeBR = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet bigIntTypeSet(bigIntegerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v1 =
          invokeRuntime("Ratio_createFromBigInteger", &retType, {bigIntTypeSet},
                        {TypedValue(bigIntTypeSet, args[0])});
      return invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                           {v1, TypedValue(retType, args[1])})
          .value;
    };
  };

  // Int mixed with Ratio Helpers
  auto makeRI = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet intTypeSet(integerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v2 =
          invokeRuntime("Ratio_createFromInt", &retType, {intTypeSet},
                        {TypedValue(intTypeSet, args[1])});
      return invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                           {TypedValue(retType, args[0]), v2})
          .value;
    };
  };

  auto makeIR = [this](const std::string &opSymbol) {
    return [this, opSymbol](auto &b, auto args) {
      ObjectTypeSet retType(ratioType);
      ObjectTypeSet intTypeSet(integerType);
      ObjectTypeSet allTypeSet = ObjectTypeSet::all();
      TypedValue v1 =
          invokeRuntime("Ratio_createFromInt", &retType, {intTypeSet},
                        {TypedValue(intTypeSet, args[0])});
      return invokeRuntime(opSymbol, &allTypeSet, {retType, retType},
                           {v1, TypedValue(retType, args[1])})
          .value;
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

  intrinsics["Add_BR"] = makeBR("Ratio_add");
  intrinsics["Add_RB"] = makeRB("Ratio_add");
  intrinsics["Sub_BR"] = makeBR("Ratio_sub");
  intrinsics["Sub_RB"] = makeRB("Ratio_sub");
  intrinsics["Mul_BR"] = makeBR("Ratio_mul");
  intrinsics["Mul_RB"] = makeRB("Ratio_mul");
  intrinsics["Div_BR"] = makeBR("Ratio_div");
  intrinsics["Div_RB"] = makeRB("Ratio_div");

  intrinsics["FCmpOGE_BR"] = makeBR("Ratio_gte");
  intrinsics["FCmpOGE_RB"] = makeRB("Ratio_gte");
  intrinsics["FCmpOGT_BR"] = makeBR("Ratio_gt");
  intrinsics["FCmpOGT_RB"] = makeRB("Ratio_gt");
  intrinsics["FCmpOLT_BR"] = makeBR("Ratio_lt");
  intrinsics["FCmpOLT_RB"] = makeRB("Ratio_lt");
  intrinsics["FCmpOLE_BR"] = makeBR("Ratio_lte");
  intrinsics["FCmpOLE_RB"] = makeRB("Ratio_lte");

  intrinsics["Add_IR"] = makeIR("Ratio_add");
  intrinsics["Add_RI"] = makeRI("Ratio_add");
  intrinsics["Sub_IR"] = makeIR("Ratio_sub");
  intrinsics["Sub_RI"] = makeRI("Ratio_sub");
  intrinsics["Mul_IR"] = makeIR("Ratio_mul");
  intrinsics["Mul_RI"] = makeRI("Ratio_mul");
  intrinsics["Div_IR"] = makeIR("Ratio_div");
  intrinsics["Div_RI"] = makeRI("Ratio_div");

  intrinsics["FCmpOGE_IR"] = makeIR("Ratio_gte");
  intrinsics["FCmpOGE_RI"] = makeRI("Ratio_gte");
  intrinsics["FCmpOGT_IR"] = makeIR("Ratio_gt");
  intrinsics["FCmpOGT_RI"] = makeRI("Ratio_gt");
  intrinsics["FCmpOLT_IR"] = makeIR("Ratio_lt");
  intrinsics["FCmpOLT_RI"] = makeRI("Ratio_lt");
  intrinsics["FCmpOLE_IR"] = makeIR("Ratio_lte");
  intrinsics["FCmpOLE_RI"] = makeRI("Ratio_lte");

  // Comparisons mixed with Double
  intrinsics["ICmpSGE_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFCmpOGE(v1, args[1]);
  };
  intrinsics["ICmpSGE_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFCmpOGE(args[0], v2);
  };
  intrinsics["ICmpSGT_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFCmpOGT(v1, args[1]);
  };
  intrinsics["ICmpSGT_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFCmpOGT(args[0], v2);
  };
  intrinsics["ICmpSLT_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFCmpOLT(v1, args[1]);
  };
  intrinsics["ICmpSLT_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFCmpOLT(args[0], v2);
  };
  intrinsics["ICmpSLE_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFCmpOLE(v1, args[1]);
  };
  intrinsics["ICmpSLE_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFCmpOLE(args[0], v2);
  };
  intrinsics["FCmpOEQ_ID"] = [this](auto &b, auto args) {
    Value *v1 = b.CreateSIToFP(args[0], types.doubleTy, "conv");
    return b.CreateFCmpOEQ(v1, args[1]);
  };
  intrinsics["FCmpOEQ_DI"] = [this](auto &b, auto args) {
    Value *v2 = b.CreateSIToFP(args[1], types.doubleTy, "conv");
    return b.CreateFCmpOEQ(args[0], v2);
  };
}

TypedValue
InvokeManager::generateIntrinsic(const IntrinsicDescription &id,
                                 const std::vector<TypedValue> &args) {
  if (id.type == CallType::Intrinsic) {
    auto block = intrinsics.find(id.symbol);
    if (block == intrinsics.end()) {
      throwInternalInconsistencyException("Intrinsic '" + id.symbol +
                                          "' does not exist.");
    }
    std::vector<llvm::Value *> argVals;
    /* no ':any' allowed in intrinsics */
    for (auto &arg : args)
      argVals.push_back(valueEncoder.unbox(arg).value);
    auto retVal = block->second(builder, argVals);

    return TypedValue(id.returnType, retVal);
  } else if (id.type == CallType::Call) {
    return invokeRuntime(id.symbol, &id.returnType, id.argTypes, args);
  }

  throwInternalInconsistencyException("Unsupported call type");
}

ObjectTypeSet InvokeManager::foldIntrinsic(const IntrinsicDescription &id,
                                           const vector<ObjectTypeSet> &args) {
  auto it = typeIntrinsics.find(id.symbol);
  if (it != typeIntrinsics.end()) {
    return it->second(args);
  }
  return id.returnType;
}

TypedValue InvokeManager::invokeRuntime(
    const std::string &fname, const ObjectTypeSet *retValType,
    const std::vector<ObjectTypeSet> &argTypes,
    const std::vector<TypedValue> &args, const bool isVariadic) {
  if (argTypes.size() > args.size())
    throwInternalInconsistencyException(
        "Internal error: To litle arguments passed");
  std::vector<llvm::Type *> llvmTypes;
  for (auto &t : argTypes) {
    llvmTypes.push_back(types.typeForType(t));
  }

  if (!isVariadic && argTypes.size() != args.size())
    throwInternalInconsistencyException("Internal error: Wrong arg count");

  FunctionType *functionType = FunctionType::get(
      retValType ? types.typeForType(*retValType) : types.voidTy, llvmTypes,
      isVariadic);

  Function *toCall =
      theModule.getFunction(fname)
          ?: Function::Create(functionType, Function::ExternalLinkage, fname,
                              theModule);

  vector<llvm::Value *> argVals;
  for (size_t i = 0; i < args.size(); i++) {
    auto &arg = args[i];
    if (i < argTypes.size()) {
      auto &argType = argTypes[i];
      if (!argType.isBoxedType()) {
        if (!arg.type.isDetermined()) {
          std::ostringstream oss;
          oss << "Internal error: Types mismatch for runtime function call: "
                 "for "
              << i << "th arg required is " << argType.toString()
              << " and found " << arg.type.toString() << ".";
          throwInternalInconsistencyException(oss.str());
        }
        argVals.push_back(valueEncoder.unbox(arg).value);
      } else {
        /* Each boxed is allowed */
        argVals.push_back(valueEncoder.box(arg).value);
      }
    } else {
      /* Varargs are always boxed */
      argVals.push_back(valueEncoder.box(arg).value);
    }
  }
  return TypedValue(
      retValType ? *retValType : ObjectTypeSet::all(),
      builder.CreateCall(toCall, argVals, std::string("call_") + fname));
}

} // namespace rt
