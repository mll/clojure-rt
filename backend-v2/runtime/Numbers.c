#include "Numbers.h"
#include "Object.h"
#include "Exceptions.h"
#include "BigInteger.h"
#include "Ratio.h"
#include "Double.h"
#include "Integer.h"
#include <limits.h>

static RTValue narrowBigInt(BigInteger *b) {
    if (mpz_fits_sint_p(b->value)) {
        int32_t val = (int32_t)mpz_get_si(b->value);
        Ptr_release(b);
        return RT_boxInt32(val);
    }
    return RT_boxPtr(b);
}

static RTValue simplifyResult(RTValue v) {
    if (getType(v) == bigIntegerType) {
        return narrowBigInt((BigInteger*)RT_unboxPtr(v));
    }
    return v;
}

static Ratio* toRatio(RTValue v) {
    objectType t = getType(v);
    if (t == ratioType) return (Ratio*)RT_unboxPtr(v);
    if (t == integerType) return Ratio_createFromInt(RT_unboxInt32(v));
    if (t == bigIntegerType) return Ratio_createFromBigInteger((BigInteger*)RT_unboxPtr(v));
    // Double to Ratio is not typically a default implicit promotion in Clojure
    return NULL;
}

static BigInteger* toBigInt(RTValue v) {
    objectType t = getType(v);
    if (t == bigIntegerType) return (BigInteger*)RT_unboxPtr(v);
    if (t == integerType) return BigInteger_createFromInt(RT_unboxInt32(v));
    return NULL;
}

double Numbers_toDouble(RTValue v) {
    objectType t = getType(v);
    if (t == doubleType) return RT_unboxDouble(v);
    if (t == integerType) return (double)RT_unboxInt32(v);
    if (t == bigIntegerType) return BigInteger_toDouble((BigInteger*)RT_unboxPtr(v));
    if (t == ratioType) return Ratio_toDouble((Ratio*)RT_unboxPtr(v));
    
    // Non-numeric type
    release(v);
    throwIllegalArgumentException_C("Operation requires a numeric argument");
    return 0.0;
}

static bool isNumeric(objectType t) {
    return t == integerType || t == doubleType || t == bigIntegerType || t == ratioType;
}

#define GENERIC_MATH_OP(NAME, OP_INT, OP_BIGINT, OP_RATIO, OP_DOUBLE) \
RTValue Numbers_##NAME(RTValue a, RTValue b) { \
    objectType ta = getType(a); \
    objectType tb = getType(b); \
    if (!isNumeric(ta) || !isNumeric(tb)) { \
        release(a); release(b); \
        throwIllegalArgumentException_C("Arithmetic operation requires numeric arguments"); \
    } \
    if (ta == doubleType || tb == doubleType) { \
        double va = Numbers_toDouble(a); \
        double vb = Numbers_toDouble(b); \
        return RT_boxDouble(va OP_DOUBLE vb); \
    } \
    if (ta == ratioType || tb == ratioType) { \
        Ratio *ra = toRatio(a); \
        Ratio *rb = toRatio(b); \
        return Ratio_##NAME(ra, rb); \
    } \
    if (ta == bigIntegerType || tb == bigIntegerType) { \
        BigInteger *ba = toBigInt(a); \
        BigInteger *bb = toBigInt(b); \
        return simplifyResult(RT_boxPtr(BigInteger_##NAME(ba, bb))); \
    } \
    int32_t va = RT_unboxInt32(a); \
    int32_t vb = RT_unboxInt32(b); \
    int32_t res; \
    if (__builtin_##OP_INT##_overflow(va, vb, &res)) { \
        BigInteger *ba = BigInteger_createFromInt(va); \
        BigInteger *bb = BigInteger_createFromInt(vb); \
        return simplifyResult(RT_boxPtr(BigInteger_##NAME(ba, bb))); \
    } \
    return RT_boxInt32(res); \
}

GENERIC_MATH_OP(add, add, add, add, +)
GENERIC_MATH_OP(sub, sub, sub, sub, -)
GENERIC_MATH_OP(mul, mul, mul, mul, *)

RTValue Numbers_div(RTValue a, RTValue b) {
    objectType ta = getType(a);
    objectType tb = getType(b);
    if (!isNumeric(ta) || !isNumeric(tb)) {
        release(a); release(b);
        throwIllegalArgumentException_C("Arithmetic operation requires numeric arguments");
    }
    if (ta == doubleType || tb == doubleType) {
        double res = Numbers_toDouble(a) / Numbers_toDouble(b);
        return RT_boxDouble(res);
    }
    // For division, we often go to Ratio even for Integers (if not divisible)
    Ratio *ra = toRatio(a);
    Ratio *rb = toRatio(b);
    return simplifyResult(Ratio_div(ra, rb));
}

#define GENERIC_CMP_OP(NAME, OP_BIGINT, OP_RATIO, OP_DOUBLE) \
bool Numbers_##NAME(RTValue a, RTValue b) { \
    objectType ta = getType(a); \
    objectType tb = getType(b); \
    if (!isNumeric(ta) || !isNumeric(tb)) { \
        release(a); release(b); \
        throwIllegalArgumentException_C("Comparison requires numeric arguments"); \
    } \
    if (ta == doubleType || tb == doubleType) { \
        bool res = Numbers_toDouble(a) OP_DOUBLE Numbers_toDouble(b); \
        return res; \
    } \
    if (ta == ratioType || tb == ratioType) { \
        Ratio *ra = toRatio(a); \
        Ratio *rb = toRatio(b); \
        return Ratio_##NAME(ra, rb); \
    } \
    if (ta == bigIntegerType || tb == bigIntegerType) { \
        BigInteger *ba = toBigInt(a); \
        BigInteger *bb = toBigInt(b); \
        return BigInteger_##NAME(ba, bb); \
    } \
    int32_t va = RT_unboxInt32(a); \
    int32_t vb = RT_unboxInt32(b); \
    return va OP_DOUBLE vb; \
}

GENERIC_CMP_OP(lt, lt, lt, <)
GENERIC_CMP_OP(lte, lte, lte, <=)
GENERIC_CMP_OP(gt, gt, gt, >)
GENERIC_CMP_OP(gte, gte, gte, >=)

bool Numbers_equiv(RTValue a, RTValue b) {
    objectType ta = getType(a);
    objectType tb = getType(b);
    if (ta == tb) {
        if (ta == integerType) return RT_unboxInt32(a) == RT_unboxInt32(b);
        if (ta == doubleType) return RT_unboxDouble(a) == RT_unboxDouble(b);
        // equals() for pointers handles refcounts
        bool res = equals(a, b);
        release(a); release(b);
        return res;
    }
    // Different types: use a comparison that handles cross-type equivalence
    // For now, let's use the same logic as comparisons but specifically for equality
    if (ta == doubleType || tb == doubleType) {
        bool res = Numbers_toDouble(a) == Numbers_toDouble(b);
        return res;
    }
    // Ratio vs Int/BigInt etc.
    Ratio *ra = toRatio(a);
    Ratio *rb = toRatio(b);
    bool res = Ratio_equals(ra, rb);
    Ptr_release(ra);
    Ptr_release(rb);
    return res;
}
#include <math.h>

#define UNARY_MATH_WRAPPER(NAME, FUNC) \
RTValue Numbers_##NAME(RTValue a) { \
    double va = Numbers_toDouble(a); \
    return RT_boxDouble(FUNC(va)); \
}

#define BINARY_MATH_WRAPPER(NAME, FUNC) \
RTValue Numbers_##NAME(RTValue a, RTValue b) { \
    if (!isNumeric(getType(a)) || !isNumeric(getType(b))) { \
        release(a); release(b); \
        throwIllegalArgumentException_C("Math operation requires numeric arguments"); \
    } \
    double va = Numbers_toDouble(a); \
    double vb = Numbers_toDouble(b); \
    return RT_boxDouble(FUNC(va, vb)); \
}

UNARY_MATH_WRAPPER(sin, sin)
UNARY_MATH_WRAPPER(cos, cos)
UNARY_MATH_WRAPPER(tan, tan)
UNARY_MATH_WRAPPER(asin, asin)
UNARY_MATH_WRAPPER(acos, acos)
UNARY_MATH_WRAPPER(atan, atan)
UNARY_MATH_WRAPPER(exp, exp)
UNARY_MATH_WRAPPER(exp2, exp2)
#ifdef __linux__
UNARY_MATH_WRAPPER(exp10, exp10)
#else
RTValue Numbers_exp10(RTValue a) {
    double va = Numbers_toDouble(a);
    return RT_boxDouble(pow(10.0, va));
}
#endif
UNARY_MATH_WRAPPER(log, log)
UNARY_MATH_WRAPPER(log10, log10)
UNARY_MATH_WRAPPER(logb, logb)
UNARY_MATH_WRAPPER(log2, log2)
UNARY_MATH_WRAPPER(sqrt, sqrt)
UNARY_MATH_WRAPPER(cbrt, cbrt)
UNARY_MATH_WRAPPER(exp1m, expm1)
UNARY_MATH_WRAPPER(log1p, log1p)
UNARY_MATH_WRAPPER(abs, fabs)

BINARY_MATH_WRAPPER(atan2, atan2)
BINARY_MATH_WRAPPER(pow, pow)
BINARY_MATH_WRAPPER(hypot, hypot)
