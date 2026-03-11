#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "../../codegen/CodeGen.h"
#include "../../types/ConstantBool.h"
#include "../../types/ConstantInteger.h"
#include "../../types/ConstantBigInteger.h"
#include "../../types/ConstantRatio.h"
#include "bytecode.pb.h"

#include <iostream>

#include "../../runtime/defines.h"
#include "../../runtime/RTValue.h"
#include "../../runtime/Object.h"
#include "../../runtime/RuntimeInterface.h"

extern "C" {
// Minimal declarations for functions provided by runtime_helpers.c
void test_initialise_memory();
void test_RuntimeInterface_cleanup();
String *test_String_create(const char *str);
Class *test_Class_create(String *name, String *simpleName, int flags, void *ext);
void test_delete_class_description(void *p);
void test_release(RTValue v);

#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void setup_mock_runtime(rt::ThreadsafeCompilerState &compState) {
    // 1. Numbers class for arithmetic
    {
        String *nameStr = test_String_create("clojure.lang.Numbers");
        String *classNameStr = test_String_create("clojure.lang.Numbers");
        Class *cls = test_Class_create(nameStr, classNameStr, 0, nullptr);
        
        ClassDescription *ext = new ClassDescription();
        ext->name = "clojure.lang.Numbers";
        
        // Add "Add" intrinsic
        IntrinsicDescription addId;
        addId.symbol = "Add";
        addId.type = CallType::Intrinsic;
        addId.argTypes = { ObjectTypeSet(integerType, false), ObjectTypeSet(integerType, false) };
        addId.returnType = ObjectTypeSet(integerType, false);
        ext->staticFns["add"].push_back(addId);
        
        // Add "GTE" intrinsic
        IntrinsicDescription gteId;
        gteId.symbol = "ICmpSGE";
        gteId.type = CallType::Intrinsic;
        gteId.argTypes = { ObjectTypeSet(integerType, false), ObjectTypeSet(integerType, false) };
        gteId.returnType = ObjectTypeSet(booleanType, false);
        ext->staticFns["gte"].push_back(gteId);
        
        // Add "Add_IB" intrinsic
        IntrinsicDescription addIBId;
        addIBId.symbol = "Add_IB";
        addIBId.type = CallType::Intrinsic;
        addIBId.argTypes = { ObjectTypeSet(integerType, false), ObjectTypeSet(bigIntegerType, false) };
        addIBId.returnType = ObjectTypeSet(bigIntegerType, false);
        ext->staticFns["add"].push_back(addIBId);

        // Add "Add_BI" intrinsic
        IntrinsicDescription addBIId;
        addBIId.symbol = "Add_BI";
        addBIId.type = CallType::Intrinsic;
        addBIId.argTypes = { ObjectTypeSet(bigIntegerType, false), ObjectTypeSet(integerType, false) };
        addBIId.returnType = ObjectTypeSet(bigIntegerType, false);
        ext->staticFns["add"].push_back(addBIId);

        // Add "BigInteger_add" intrinsic

        IntrinsicDescription addBBId;
        addBBId.symbol = "BigInteger_add";
        addBBId.type = CallType::Intrinsic;
        addBBId.argTypes = { ObjectTypeSet(bigIntegerType, false), ObjectTypeSet(bigIntegerType, false) };
        addBBId.returnType = ObjectTypeSet(bigIntegerType, false);
        ext->staticFns["add"].push_back(addBBId);

        // Add "Ratio_add" intrinsic
        IntrinsicDescription addRRId;
        addRRId.symbol = "Ratio_add";
        addRRId.type = CallType::Intrinsic;
        addRRId.argTypes = { ObjectTypeSet(ratioType, false), ObjectTypeSet(ratioType, false) };
        addRRId.returnType = ObjectTypeSet(ratioType, false);
        ext->staticFns["add"].push_back(addRRId);
        
        // Add "GTE" for BigInt
        IntrinsicDescription gteBBId;
        gteBBId.symbol = "BigInteger_gte";
        gteBBId.type = CallType::Intrinsic;
        gteBBId.argTypes = { ObjectTypeSet(bigIntegerType, false), ObjectTypeSet(bigIntegerType, false) };
        gteBBId.returnType = ObjectTypeSet(booleanType, false);
        ext->staticFns["gte"].push_back(gteBBId);

        cls->compilerExtension = ext;

        cls->compilerExtensionDestructor = test_delete_class_description;
        compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
    }
}

static void test_constant_arithmetic_getType(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    
    CodeGen codegen("test", compState);

    // (clojure.lang.Numbers/add 1 2)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("1");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("2");

    ObjectTypeSet resultType = codegen.getType(callNode, ObjectTypeSet::all());
    
    assert_true(resultType.isDetermined());
    assert_int_equal(integerType, resultType.determinedType());
    assert_non_null(resultType.getConstant());
    
    auto *cons = dynamic_cast<ConstantInteger *>(resultType.getConstant());
    assert_non_null(cons);
    assert_int_equal(3, cons->value);
}

static void test_constant_comparison_getType(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    
    CodeGen codegen("test", compState);

    // (clojure.lang.Numbers/gte 10 5)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("gte");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("10");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("5");

    ObjectTypeSet resultType = codegen.getType(callNode, ObjectTypeSet::all());
    
    assert_true(resultType.isDetermined());
    assert_int_equal(booleanType, resultType.determinedType());
    assert_non_null(resultType.getConstant());
    
    auto *cons = dynamic_cast<ConstantBoolean *>(resultType.getConstant());
    assert_non_null(cons);
    assert_true(cons->value);
}

static void test_bigint_arithmetic_getType(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    CodeGen codegen("test", compState);

    // (clojure.lang.Numbers/add 100000000000000000000N 1)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->set_tag("long");
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("100000000000000000000"); // BigInt

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->set_tag("long");
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("1");

    ObjectTypeSet resultType = codegen.getType(callNode, ObjectTypeSet::all());
    
    assert_true(resultType.isDetermined());
    assert_int_equal(bigIntegerType, resultType.determinedType());
    assert_non_null(resultType.getConstant());
    
    auto *cons = dynamic_cast<ConstantBigInteger *>(resultType.getConstant());
    assert_non_null(cons);
    assert_string_equal("100000000000000000001", cons->toString().c_str());
}

static void test_ratio_arithmetic_getType(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    CodeGen codegen("test", compState);

    // (clojure.lang.Numbers/add 1/2 1/3)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("1/2");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("1/3");

    ObjectTypeSet resultType = codegen.getType(callNode, ObjectTypeSet::all());
    
    assert_true(resultType.isDetermined());
    assert_int_equal(ratioType, resultType.determinedType());
    assert_non_null(resultType.getConstant());
    
    auto *cons = dynamic_cast<ConstantRatio *>(resultType.getConstant());
    assert_non_null(cons);
    assert_string_equal("5/6", cons->toString().c_str());
}

static void test_bigint_comparison_getType(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_mock_runtime(compState);
    CodeGen codegen("test", compState);

    // (clojure.lang.Numbers/gte 100000000000000000001N 100000000000000000000N)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("gte");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->mutable_subnode()->mutable_const_()->set_val("100000000000000000001");

    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    arg2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->mutable_subnode()->mutable_const_()->set_val("100000000000000000000");

    ObjectTypeSet resultType = codegen.getType(callNode, ObjectTypeSet::all());
    
    assert_true(resultType.isDetermined());
    assert_int_equal(booleanType, resultType.determinedType());
    assert_non_null(resultType.getConstant());
    
    auto *cons = dynamic_cast<ConstantBoolean *>(resultType.getConstant());
    assert_non_null(cons);
    assert_true(cons->value);
}


int main(void) {
    test_initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_constant_arithmetic_getType),
        cmocka_unit_test(test_constant_comparison_getType),
        cmocka_unit_test(test_bigint_arithmetic_getType),
        cmocka_unit_test(test_ratio_arithmetic_getType),
        cmocka_unit_test(test_bigint_comparison_getType),
    };


    int result = cmocka_run_group_tests(tests, NULL, NULL);
    test_RuntimeInterface_cleanup();
    return result;
}
