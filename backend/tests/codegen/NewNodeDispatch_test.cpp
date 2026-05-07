#include "../../codegen/CodeGen.h"
#include "../../runtime/Function.h"
#include "../../runtime/Keyword.h"
#include "../../runtime/String.h"
#include "../../runtime/Var.h"
#include "../../runtime/StringBuilder.h"
#include "jit/JITEngine.h"
#include "../../tools/EdnParser.h"
#include "runtime/Ebr.h"
#include "runtime/RTValue.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_new_node_dynamic_dispatch(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        rt::JITEngine engine;
        auto &compState = engine.threadsafeState;

        // 1. Manually register StringBuilder class metadata
        ::Class *sbClass = Class_create(
            String_create("java.lang.StringBuilder"),
            String_create("java.lang.StringBuilder"),
            0, NULL);
        sbClass->registerId = stringBuilderType;
        
        ClassDescription *ext = new ClassDescription();
        IntrinsicDescription ctor;
        ctor.type = CallType::Call;
        ctor.symbol = "StringBuilder_create";
        ctor.argTypes = { ObjectTypeSet(stringType, false) };
        ctor.returnType = ObjectTypeSet(stringBuilderType, false);
        ext->constructors.push_back(ctor);
        
        sbClass->compilerExtension = ext;
        sbClass->compilerExtensionDestructor = delete_class_description;
        compState.classRegistry.registerObject("java.lang.StringBuilder", sbClass);

        // 2. Create (let [x "Hello"] (new java.lang.StringBuilder x))
        // x in let is often inferred as ANY if we don't know better, 
        // but here it might be inferred as String. 
        // To force dynamic dispatch, we'll use a local that is intentionally opaque.
        
        Node letNode;
        letNode.set_op(opLet);
        auto *ln = letNode.mutable_subnode()->mutable_let();
        auto *b = ln->add_bindings();
        b->set_op(opBinding);
        auto *bn = b->mutable_subnode()->mutable_binding();
        bn->set_name("x");
        bn->set_local(localTypeLet);
        
        auto *init = bn->mutable_init();
        init->set_op(opConst);
        init->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
        init->mutable_subnode()->mutable_const_()->set_val("Hello");

        auto *body = ln->mutable_body();
        body->set_op(opNew);
        auto *nn = body->mutable_subnode()->mutable_new_();
        nn->mutable_class_()->set_op(opConst);
        nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeClass);
        nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_val("java.lang.StringBuilder");
        
        auto *arg = nn->add_args();
        arg->set_op(opLocal);
        arg->mutable_subnode()->mutable_local()->set_name("x");
        arg->mutable_subnode()->mutable_local()->set_local(localTypeLet);

        // 3. Compile and Run
        auto res = engine.compileAST(letNode, "new_test").get();
        RTValue (*runFn)() = res.address.toPtr<RTValue (*)()>();
        RTValue result = runFn();
        
        assert_int_equal(getType(result), stringBuilderType);
        
        // Convert to string to verify content
        StringBuilder *sb = (StringBuilder *)RT_unboxPtr(result);
        String *s = StringBuilder_toString(sb);
        assert_string_equal(String_c_str(s), "Hello");
        Ptr_release(s);

        // 4. Cleanup
        engine.retireModule("new_test");
    });
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_new_node_dynamic_dispatch),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
