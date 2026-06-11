#include "../../codegen/CodeGen.h"
#include "../../codegen/invoke/InvokeManager.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <iostream>
#include <string>

extern "C" {
#include "../../runtime/BigInteger.h"
#include "../../runtime/Ratio.h"
#include "../../runtime/String.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Helper to quickly fold
static ObjectTypeSet doFold(InvokeManager &mgr, const string &symbol, ObjectTypeSet arg1, ObjectTypeSet arg2) {
    IntrinsicDescription id;
    id.symbol = symbol;
    id.type = CallType::Intrinsic;
    return mgr.foldIntrinsic(id, {arg1, arg2});
}

// Helper to check big integer value
static bool checkZ(const ObjectTypeSet &t, long expected) {
    if (!t.getConstant()) {
        printf("checkZ failed: getConstant() is null\n");
        return false;
    }
    if (t.contains(integerType)) {
        auto *ci = dynamic_cast<ConstantInteger*>(t.getConstant());
        if (!ci) return false;
        return ci->value == expected;
    } else if (t.contains(bigIntegerType)) {
        mpz_t expectedZ;
        mpz_init_set_si(expectedZ, expected);
        auto *bz = dynamic_cast<ConstantBigInteger*>(t.getConstant());
        if (!bz) {
            mpz_clear(expectedZ);
            return false;
        }
        bool eq = (mpz_cmp(bz->value, expectedZ) == 0);
        mpz_clear(expectedZ);
        return eq;
    }
    return false;
}

// Helper to check ratio value
static bool checkQ(const ObjectTypeSet &t, long num, long den) {
    if (!t.getConstant()) return false;
    mpq_t expectedQ;
    mpq_init(expectedQ);
    mpq_set_si(expectedQ, num, den);
    mpq_canonicalize(expectedQ);

    bool eq = false;
    if (t.contains(ratioType)) {
        auto *cr = dynamic_cast<ConstantRatio*>(t.getConstant());
        if (cr) {
            eq = (mpq_cmp(cr->value, expectedQ) == 0);
        }
    } else if (t.contains(bigIntegerType)) {
        auto *bz = dynamic_cast<ConstantBigInteger*>(t.getConstant());
        if (bz) {
            mpq_t bzq;
            mpq_init(bzq);
            mpq_set_z(bzq, bz->value);
            eq = (mpq_cmp(bzq, expectedQ) == 0);
            mpq_clear(bzq);
        }
    } else if (t.contains(integerType)) {
        auto *ci = dynamic_cast<ConstantInteger*>(t.getConstant());
        if (ci) {
            mpq_t ciq;
            mpq_init(ciq);
            mpq_set_si(ciq, ci->value, 1);
            eq = (mpq_cmp(ciq, expectedQ) == 0);
            mpq_clear(ciq);
        }
    }
    mpq_clear(expectedQ);
    return eq;
}

static void setup_test_classes(rt::ThreadsafeCompilerState &compState) {
    const char* classes[] = {
        "clojure.lang.BigInt",
        "clojure.lang.Ratio",
        "clojure.lang.Numbers",
        "java.lang.Math"
    };
    for (const char* cname : classes) {
        String *nameStr = String_create(cname);
        Ptr_retain(nameStr);
        Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
        compState.classRegistry.registerObject(cname, cls);
    }
}

static void test_math_constant_folding(void **state) {
    (void)state;
    ThreadsafeCompilerState compState;
    setup_test_classes(compState);
    CodeGen cg("test", compState);
    InvokeManager &mgr = cg.getInvokeManager();

    // Constant Integer
    ObjectTypeSet i1(integerType, false, new ConstantInteger(10));
    ObjectTypeSet i2(integerType, false, new ConstantInteger(2));
    ObjectTypeSet iz(integerType, false, new ConstantInteger(0));

    auto rAdd = doFold(mgr, "Add", i1, i2);
    assert_int_equal(dynamic_cast<ConstantInteger*>(rAdd.getConstant())->value, 12);

    auto rSub = doFold(mgr, "Sub", i1, i2);
    assert_int_equal(dynamic_cast<ConstantInteger*>(rSub.getConstant())->value, 8);

    auto rMul = doFold(mgr, "Mul", i1, i2);
    assert_int_equal(dynamic_cast<ConstantInteger*>(rMul.getConstant())->value, 20);

    auto rDiv = doFold(mgr, "Div", i1, i2);
    assert_int_equal(dynamic_cast<ConstantInteger*>(rDiv.getConstant())->value, 5);

    auto rDivZ = doFold(mgr, "Div", i1, iz);
    assert_null(rDivZ.getConstant()); // Division by zero yields dynamic or non-const

    // Constant Double
    ObjectTypeSet d1(doubleType, false, new ConstantDouble(5.0));
    ObjectTypeSet d2(doubleType, false, new ConstantDouble(2.0));
    ObjectTypeSet dz(doubleType, false, new ConstantDouble(0.0));

    auto rFAdd = doFold(mgr, "FAdd", d1, d2);
    assert_true(rFAdd.contains(doubleType));
    assert_float_equal(dynamic_cast<ConstantDouble*>(rFAdd.getConstant())->value, 7.0, 0.001);

    auto rFSub = doFold(mgr, "FSub", d1, d2);
    assert_float_equal(dynamic_cast<ConstantDouble*>(rFSub.getConstant())->value, 3.0, 0.001);

    auto rFMul = doFold(mgr, "FMul", d1, d2);
    assert_float_equal(dynamic_cast<ConstantDouble*>(rFMul.getConstant())->value, 10.0, 0.001);

    auto rFDiv = doFold(mgr, "FDiv", d1, d2);
    assert_float_equal(dynamic_cast<ConstantDouble*>(rFDiv.getConstant())->value, 2.5, 0.001);
    
    auto rFDivZ = doFold(mgr, "FDiv", d1, dz);
    assert_null(rFDivZ.getConstant());

    // BigInteger
    mpz_t b1z, b2z, b0z;
    mpz_init_set_si(b1z, 100);
    mpz_init_set_si(b2z, 20);
    mpz_init_set_si(b0z, 0);
    ObjectTypeSet b1(bigIntegerType, false, new ConstantBigInteger(b1z));
    ObjectTypeSet b2(bigIntegerType, false, new ConstantBigInteger(b2z));
    ObjectTypeSet b0(bigIntegerType, false, new ConstantBigInteger(b0z));

    assert_true(checkZ(doFold(mgr, "BigInteger_add", b1, b2), 120));
    assert_true(checkZ(doFold(mgr, "BigInteger_sub", b1, b2), 80));
    assert_true(checkZ(doFold(mgr, "BigInteger_mul", b1, b2), 2000));
    assert_true(checkQ(doFold(mgr, "BigInteger_div", b1, b2), 5, 1));
    assert_null(doFold(mgr, "BigInteger_div", b1, b0).getConstant());

    // Ratio
    mpq_t q1z, q2z, q0z;
    mpq_init(q1z); mpq_init(q2z); mpq_init(q0z);
    mpz_set_si(mpq_numref(q1z), 1); mpz_set_si(mpq_denref(q1z), 2); mpq_canonicalize(q1z); // 1/2
    mpz_set_si(mpq_numref(q2z), 1); mpz_set_si(mpq_denref(q2z), 4); mpq_canonicalize(q2z); // 1/4
    ObjectTypeSet q1(ratioType, false, new ConstantRatio(q1z));
    ObjectTypeSet q2(ratioType, false, new ConstantRatio(q2z));
    ObjectTypeSet q0(ratioType, false, new ConstantRatio(q0z));

    assert_true(checkQ(doFold(mgr, "Ratio_add", q1, q2), 3, 4));
    assert_true(checkQ(doFold(mgr, "Ratio_sub", q1, q2), 1, 4));
    assert_true(checkQ(doFold(mgr, "Ratio_mul", q1, q2), 1, 8));
    assert_true(checkQ(doFold(mgr, "Ratio_div", q1, q2), 2, 1));
    assert_null(doFold(mgr, "Ratio_div", q1, q0).getConstant());

    // Mixed Z/I
    assert_true(checkZ(doFold(mgr, "Add_IB", i1, b2), 30));
    assert_true(checkZ(doFold(mgr, "Add_BI", b2, i1), 30));
    assert_true(checkZ(doFold(mgr, "Sub_IB", i1, b2), -10));
    assert_true(checkZ(doFold(mgr, "Sub_BI", b2, i1), 10));
    assert_true(checkZ(doFold(mgr, "Mul_IB", i1, b2), 200));
    assert_true(checkZ(doFold(mgr, "Mul_BI", b2, i1), 200));
    assert_true(checkQ(doFold(mgr, "Div_IB", i1, b2), 1, 2));
    assert_true(checkQ(doFold(mgr, "Div_BI", b2, i1), 2, 1));
    assert_null(doFold(mgr, "Div_IB", i1, b0).getConstant());
    assert_null(doFold(mgr, "Div_BI", b2, iz).getConstant());

    // Mixed D/I
    assert_true(doFold(mgr, "Add_ID", i1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Add_DI", d2, i1).contains(doubleType));
    assert_true(doFold(mgr, "Sub_ID", i1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Sub_DI", d2, i1).contains(doubleType));
    assert_true(doFold(mgr, "Mul_ID", i1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Mul_DI", d2, i1).contains(doubleType));
    assert_true(doFold(mgr, "Div_ID", i1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Div_DI", d2, i1).contains(doubleType));

    // Mixed B/D
    assert_true(doFold(mgr, "Add_BD", b1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Add_DB", d2, b1).contains(doubleType));
    assert_true(doFold(mgr, "Sub_BD", b1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Sub_DB", d2, b1).contains(doubleType));
    assert_true(doFold(mgr, "Mul_BD", b1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Mul_DB", d2, b1).contains(doubleType));
    assert_true(doFold(mgr, "Div_BD", b1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Div_DB", d2, b1).contains(doubleType));

    // Mixed R/D
    assert_true(doFold(mgr, "Add_RD", q1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Add_DR", d2, q1).contains(doubleType));
    assert_true(doFold(mgr, "Sub_RD", q1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Sub_DR", d2, q1).contains(doubleType));
    assert_true(doFold(mgr, "Mul_RD", q1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Mul_DR", d2, q1).contains(doubleType));
    assert_true(doFold(mgr, "Div_RD", q1, d2).contains(doubleType));
    assert_true(doFold(mgr, "Div_DR", d2, q1).contains(doubleType));

    // Mixed B/R
    assert_true(checkQ(doFold(mgr, "Add_BR", b2, q1), 41, 2));
    assert_true(checkQ(doFold(mgr, "Add_RB", q1, b2), 41, 2));
    assert_true(checkQ(doFold(mgr, "Sub_BR", b2, q1), 39, 2));
    assert_true(checkQ(doFold(mgr, "Sub_RB", q1, b2), -39, 2));
    assert_true(checkQ(doFold(mgr, "Mul_BR", b2, q1), 10, 1));
    assert_true(checkQ(doFold(mgr, "Mul_RB", q1, b2), 10, 1));
    assert_true(checkQ(doFold(mgr, "Div_BR", b2, q1), 40, 1));
    assert_true(checkQ(doFold(mgr, "Div_RB", q1, b2), 1, 40));
    assert_null(doFold(mgr, "Div_BR", b2, q0).getConstant());
    assert_null(doFold(mgr, "Div_RB", q1, b0).getConstant());

    // Mixed I/R
    assert_true(checkQ(doFold(mgr, "Add_IR", i2, q1), 5, 2));
    assert_true(checkQ(doFold(mgr, "Add_RI", q1, i2), 5, 2));
    assert_true(checkQ(doFold(mgr, "Sub_IR", i2, q1), 3, 2));
    assert_true(checkQ(doFold(mgr, "Sub_RI", q1, i2), -3, 2));
    assert_true(checkQ(doFold(mgr, "Mul_IR", i2, q1), 1, 1));
    assert_true(checkQ(doFold(mgr, "Mul_RI", q1, i2), 1, 1));
    assert_true(checkQ(doFold(mgr, "Div_IR", i2, q1), 4, 1));
    assert_true(checkQ(doFold(mgr, "Div_RI", q1, i2), 1, 4));
    assert_null(doFold(mgr, "Div_IR", i2, q0).getConstant());
    assert_null(doFold(mgr, "Div_RI", q1, iz).getConstant());

    mpz_clear(b1z); mpz_clear(b2z); mpz_clear(b0z);
    mpq_clear(q1z); mpq_clear(q2z); mpq_clear(q0z);
}

// Helper for codegen intrinsics
static void doCodegen(InvokeManager &mgr, CodeGen &cg, const string &op, const ObjectTypeSet &t1, const ObjectTypeSet &t2) {
    auto &builder = mgr.builder;
    llvm::FunctionType *ft = llvm::FunctionType::get(builder.getVoidTy(), false);
    llvm::Function *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", mgr.getLLVMModule());
    llvm::BasicBlock *bb = llvm::BasicBlock::Create(builder.getContext(), "entry", f);
    builder.SetInsertPoint(bb);
    
    // Create dummy values for arguments
    llvm::Value *v1 = llvm::UndefValue::get(t1.contains(doubleType) ? builder.getDoubleTy() : builder.getInt64Ty());
    llvm::Value *v2 = llvm::UndefValue::get(t2.contains(doubleType) ? builder.getDoubleTy() : builder.getInt64Ty());

    // Actually, BigInteger and Ratio are pointers. So we need pointers for them.
    if (t1.contains(bigIntegerType) || t1.contains(ratioType) || t1.isDynamic()) {
        v1 = llvm::UndefValue::get(builder.getPtrTy());
    }
    if (t2.contains(bigIntegerType) || t2.contains(ratioType) || t2.isDynamic()) {
        v2 = llvm::UndefValue::get(builder.getPtrTy());
    }

    IntrinsicDescription id;
    id.symbol = op;
    id.type = CallType::Intrinsic;
    // generateIntrinsic will execute the codegen lambda which inserts instructions into 'bb'
    mgr.generateIntrinsic(id, {TypedValue(t1, v1), TypedValue(t2, v2)}, nullptr);
    
    // We don't need to verify the IR, just that it didn't crash.
    f->eraseFromParent();
}


static void test_math_codegen(void **state) {
    (void)state;
    ThreadsafeCompilerState compState;
    setup_test_classes(compState);
    CodeGen cg("test", compState);
    InvokeManager &mgr = cg.getInvokeManager();

    ObjectTypeSet i(integerType);
    ObjectTypeSet d(doubleType);
    ObjectTypeSet b(bigIntegerType);
    ObjectTypeSet r(ratioType);
    
    // Checked Ops
    doCodegen(mgr, cg, "Add", i, i);
    doCodegen(mgr, cg, "Sub", i, i);
    doCodegen(mgr, cg, "Mul", i, i);

    // Float Ops
    doCodegen(mgr, cg, "FAdd", d, d);
    doCodegen(mgr, cg, "FSub", d, d);
    doCodegen(mgr, cg, "FMul", d, d);
    doCodegen(mgr, cg, "FDiv", d, d);
    doCodegen(mgr, cg, "Div", i, i);

    // Z/Q
    doCodegen(mgr, cg, "BigInteger_add", b, b);
    doCodegen(mgr, cg, "BigInteger_sub", b, b);
    doCodegen(mgr, cg, "BigInteger_mul", b, b);
    doCodegen(mgr, cg, "BigInteger_div", b, b);
    doCodegen(mgr, cg, "Integer_div", i, i);
    
    doCodegen(mgr, cg, "Ratio_add", r, r);
    doCodegen(mgr, cg, "Ratio_sub", r, r);
    doCodegen(mgr, cg, "Ratio_mul", r, r);
    doCodegen(mgr, cg, "Ratio_div", r, r);

    // Mixed
    doCodegen(mgr, cg, "Add_ID", i, d);
    doCodegen(mgr, cg, "Add_DI", d, i);
    doCodegen(mgr, cg, "Sub_ID", i, d);
    doCodegen(mgr, cg, "Sub_DI", d, i);
    doCodegen(mgr, cg, "Mul_ID", i, d);
    doCodegen(mgr, cg, "Mul_DI", d, i);
    doCodegen(mgr, cg, "Div_ID", i, d);
    doCodegen(mgr, cg, "Div_DI", d, i);

    doCodegen(mgr, cg, "Add_IB", i, b);
    doCodegen(mgr, cg, "Add_BI", b, i);
    doCodegen(mgr, cg, "Sub_IB", i, b);
    doCodegen(mgr, cg, "Sub_BI", b, i);
    doCodegen(mgr, cg, "Mul_IB", i, b);
    doCodegen(mgr, cg, "Mul_BI", b, i);
    doCodegen(mgr, cg, "Div_IB", i, b);
    doCodegen(mgr, cg, "Div_BI", b, i);

    doCodegen(mgr, cg, "Add_BD", b, d);
    doCodegen(mgr, cg, "Add_DB", d, b);
    doCodegen(mgr, cg, "Sub_BD", b, d);
    doCodegen(mgr, cg, "Sub_DB", d, b);
    doCodegen(mgr, cg, "Mul_BD", b, d);
    doCodegen(mgr, cg, "Mul_DB", d, b);
    doCodegen(mgr, cg, "Div_BD", b, d);
    doCodegen(mgr, cg, "Div_DB", d, b);

    doCodegen(mgr, cg, "Add_RD", r, d);
    doCodegen(mgr, cg, "Add_DR", d, r);
    doCodegen(mgr, cg, "Sub_RD", r, d);
    doCodegen(mgr, cg, "Sub_DR", d, r);
    doCodegen(mgr, cg, "Mul_RD", r, d);
    doCodegen(mgr, cg, "Mul_DR", d, r);
    doCodegen(mgr, cg, "Div_RD", r, d);
    doCodegen(mgr, cg, "Div_DR", d, r);

    doCodegen(mgr, cg, "Add_BR", b, r);
    doCodegen(mgr, cg, "Add_RB", r, b);
    doCodegen(mgr, cg, "Sub_BR", b, r);
    doCodegen(mgr, cg, "Sub_RB", r, b);
    doCodegen(mgr, cg, "Mul_BR", b, r);
    doCodegen(mgr, cg, "Mul_RB", r, b);
    doCodegen(mgr, cg, "Div_BR", b, r);
    doCodegen(mgr, cg, "Div_RB", r, b);

    doCodegen(mgr, cg, "Add_IR", i, r);
    doCodegen(mgr, cg, "Add_RI", r, i);
    doCodegen(mgr, cg, "Sub_IR", i, r);
    doCodegen(mgr, cg, "Sub_RI", r, i);
    doCodegen(mgr, cg, "Mul_IR", i, r);
    doCodegen(mgr, cg, "Mul_RI", r, i);
    doCodegen(mgr, cg, "Div_IR", i, r);
    doCodegen(mgr, cg, "Div_RI", r, i);
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_math_constant_folding),
        cmocka_unit_test(test_math_codegen),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
