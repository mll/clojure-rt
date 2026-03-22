#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include "../../runtime/BigInteger.h"
#include "../../runtime/String.h"
#include <cmocka.h>

void delete_class_description(void *ptr);
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void setup_arithmetic_runtime(rt::ThreadsafeCompilerState &compState) {
    // We need BigInt and Numbers
    {
        String *nameStr = String_create("clojure.lang.BigInt");
        Ptr_retain(nameStr);
        Class *cls = Class_create(nameStr, nameStr, 0, nullptr); 
        compState.classRegistry.registerObject("clojure.lang.BigInt", cls);
    }
    
    {
        String *nameStr = String_create("clojure.lang.Numbers");
        Ptr_retain(nameStr);
        Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
        
        ClassDescription *ext = new ClassDescription();
        ext->name = "clojure.lang.Numbers";
        
        // Add_IB intrinsic: int + bigint -> bigint
        IntrinsicDescription addIB;
        addIB.symbol = "Add_IB"; 
        addIB.type = CallType::Intrinsic;
        addIB.argTypes = { ObjectTypeSet(integerType, false), ObjectTypeSet(bigIntegerType, false) };
        addIB.returnType = ObjectTypeSet(bigIntegerType, false);
        ext->staticFns["add"].push_back(addIB);

        cls->compilerExtension = ext;
        cls->compilerExtensionDestructor = delete_class_description;
        compState.classRegistry.registerObject("clojure.lang.Numbers", cls);
    }
}

static void test_jit_arithmetic_ib_regression(void **state) {
    (void)state;
    rt::ThreadsafeCompilerState compState;
    setup_arithmetic_runtime(compState);
    JITEngine engine(compState);

    // (+ 5 3N)
    Node callNode;
    callNode.set_op(opStaticCall);
    auto *sc = callNode.mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");

    // Arg 1: 5 (boxed int)
    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    auto *c1 = arg1->mutable_subnode()->mutable_const_();
    c1->set_val("5");
    c1->set_type(ConstNode_ConstType_constTypeNumber);
    arg1->set_tag("long"); 

    // Arg 2: 3N (boxed bigint)
    auto *arg2 = sc->add_args();
    arg2->set_op(opConst);
    auto *c2 = arg2->mutable_subnode()->mutable_const_();
    c2->set_val("3");
    c2->set_type(ConstNode_ConstType_constTypeNumber);
    arg2->set_tag("clojure.lang.BigInt");

    try {
        auto resCall = engine.compileAST(callNode, "__test_arith_regression", llvm::OptimizationLevel::O0, false).get().address;
        RTValue result = resPtrToValue(resCall);
        
        assert_true(getType(result) == bigIntegerType);
        BigInteger* resultBI = (BigInteger*)RT_unboxPtr(result);
        
        // Use mpz_cmp_si to verify result is exactly 8
        if (mpz_cmp_si(resultBI->value, 8) != 0) {
            char* s = mpz_get_str(NULL, 10, resultBI->value);
            fprintf(stderr, "Expected 8, got %s\n", s);
            free(s);
            assert_true(false);
        }
        
        release(result); 
    } catch (const std::exception &e) {
        fprintf(stderr, "Arithmetic regression failure: %s\n", e.what());
        assert_true(false);
    }
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_jit_arithmetic_ib_regression),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);
    printf("Tests finished with result: %d. Cleaning up...\n", result);
    RuntimeInterface_cleanup();
    printf("Cleanup finished.\n");
    return result;
}
