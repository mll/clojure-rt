#include "../RuntimeHeaders.h"
#include "../jit/JITEngine.h"
#include "../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include "../tools/EdnParser.h"
#include <fstream>
#include <iostream>
#include <string>

extern "C" {
#include "../runtime/RuntimeInterface.h"
#include "../runtime/tests/TestTools.h"
#include "../runtime/Var.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

#include "../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// A custom consuming function that releases its arguments and then throws.
extern "C" RTValue repro_consuming_fn(RTValue a, RTValue b) {
    release(a);
    release(b);
    throwIllegalArgumentException_C("Releasing arguments and throwing");
    return RT_boxNil();
}

static void setup_repro_compiler_state(rt::ThreadsafeCompilerState &compState) {
    String *className = String_create("repro.Core");
    Ptr_retain(className);
    
    auto *ext = new ClassDescription();
    ext->name = "repro.Core";
    
    IntrinsicDescription fnDesc;
    fnDesc.symbol = "repro_consuming_fn";
    fnDesc.type = CallType::Call;
    fnDesc.argTypes = {ObjectTypeSet::dynamicType(), ObjectTypeSet::dynamicType()};
    fnDesc.returnType = ObjectTypeSet::dynamicType();
    ext->staticFns["fail"] = {fnDesc};

    Class *cls = Class_create(className, className, 0, nullptr);
    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject("repro.Core", cls);
}

static void test_double_release_uaf_repro(void **state_arg) {
    (void)state_arg;
    
    printf("--- Starting Double Release Repro Test ---\n");
    
    rt::ThreadsafeCompilerState compState;
    rt::JITEngine engine(compState);
    setup_repro_compiler_state(compState);

    // Create two strings to be consumed
    RTValue s1 = RT_boxPtr(String_create("string1"));
    RTValue s2 = RT_boxPtr(String_create("string2"));
    
    printf("Initial refcounts: s1=%lu, s2=%lu\n", 
           (unsigned long)Object_getRawRefCount((Object*)RT_unboxPtr(s1)),
           (unsigned long)Object_getRawRefCount((Object*)RT_unboxPtr(s2)));

    // Create (repro.Core/fail s1 s2)
    Node topCall;
    topCall.set_op(opStaticCall);
    topCall.mutable_env()->set_context("repro");
    auto *sc = topCall.mutable_subnode()->mutable_staticcall();
    sc->set_class_("repro.Core");
    sc->set_method("fail");

    auto *arg1 = sc->add_args();
    arg1->set_op(opConst);
    arg1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    arg1->mutable_subnode()->mutable_const_()->set_val("string1");
    // We need to make sure the JIT uses our created s1. 
    // In a real scenario, these would be in the constant pool.
    
    // Actually, it's easier to use VarNode to pass them as if they are from global vars.
    Var *v1 = Var_create(Keyword_create(String_create("user/v1")));
    Ptr_retain(v1); // Retain so it survives bindRoot consumption
    Var_bindRoot(v1, s1);
    compState.varRegistry.registerObject("user/v1", v1);
    
    Var *v2 = Var_create(Keyword_create(String_create("user/v2")));
    Ptr_retain(v2);
    Var_bindRoot(v2, s2);
    compState.varRegistry.registerObject("user/v2", v2);

    arg1->set_op(opVar);
    arg1->mutable_subnode()->mutable_var()->set_var("#'user/v1");

    auto *arg2 = sc->add_args();
    arg2->set_op(opVar);
    arg2->mutable_subnode()->mutable_var()->set_var("#'user/v2");

    printf("Compiling repro AST...\n");
    auto res = engine.compileAST(topCall, "repro", llvm::OptimizationLevel::O0, true).get().address;
    typedef RTValue (*FnPtr)();
    FnPtr func = (FnPtr)res.getValue();
    
    printf("Executing JIT function (expecting double release crash if not fixed)...\n");
    bool thrown = false;
    try {
        func();
    } catch (const LanguageException &e) {
        thrown = true;
        printf("Caught LanguageException: %s\n", getExceptionString(e).c_str());
    } catch (...) {
        thrown = true;
        printf("Caught unknown exception\n");
    }
    
    assert_true(thrown);
    
    // If it didn't crash yet, it means it's likely fixed.
    printf("Final refcounts (should be >0 if fixed): s1=%lu, s2=%lu\n", 
           (unsigned long)Object_getRawRefCount((Object*)RT_unboxPtr(s1)),
           (unsigned long)Object_getRawRefCount((Object*)RT_unboxPtr(s2)));

    engine.invalidate("repro_module");
    printf("--- Double Release Repro Test Finished ---\n");
}

int main(void) {
    initialise_memory();
    Var_initialize();
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_double_release_uaf_repro),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);
    
    Var_cleanup();
    RuntimeInterface_cleanup();
    return result;
}
