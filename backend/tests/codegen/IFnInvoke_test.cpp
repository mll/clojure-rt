#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../codegen/invoke/InvokeManager.h"
#include "../../runtime/Numbers.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_keyword_invoke(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        JITEngine engine;

        // AST: (:foo {:foo 42}) -> 42
        Node node;
        node.set_op(opInvoke);
        auto *inv = node.mutable_subnode()->mutable_invoke();
        
        // Function part: :foo
        auto *fnNode = inv->mutable_fn();
        fnNode->set_op(opConst);
        auto *cFn = fnNode->mutable_subnode()->mutable_const_();
        cFn->set_type(ConstNode_ConstType_constTypeKeyword);
        cFn->set_val(":foo");
        
        // Arg 0: {:foo 42}
        auto *argMapNode = inv->add_args();
        argMapNode->set_op(opMap);
        auto *m = argMapNode->mutable_subnode()->mutable_map();
        
        // Key: :foo
        auto *k = m->add_keys();
        k->set_op(opConst);
        auto *ck = k->mutable_subnode()->mutable_const_();
        ck->set_type(ConstNode_ConstType_constTypeKeyword);
        ck->set_val(":foo");
        
        // Val: 42
        auto *v = m->add_vals();
        v->set_op(opConst);
        auto *cv = v->mutable_subnode()->mutable_const_();
        cv->set_type(ConstNode_ConstType_constTypeNumber);
        cv->set_val("42");

        auto res = engine.compileAST(node, "keyword_invoke_test").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue val = func();
        
        assert_int_equal(42, RT_unboxInt32(val));
        release(val);
    });
}

static void test_map_invoke(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        JITEngine engine;

        // AST: ({:foo 42} :foo) -> 42
        Node node;
        node.set_op(opInvoke);
        auto *inv = node.mutable_subnode()->mutable_invoke();
        
        // Function part: {:foo 42}
        auto *fnNode = inv->mutable_fn();
        fnNode->set_op(opMap);
        auto *m = fnNode->mutable_subnode()->mutable_map();
        auto *k = m->add_keys();
        k->set_op(opConst);
        k->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
        k->mutable_subnode()->mutable_const_()->set_val(":foo");
        auto *v = m->add_vals();
        v->set_op(opConst);
        v->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        v->mutable_subnode()->mutable_const_()->set_val("42");
        
        // Arg 0: :foo
        auto *arg0 = inv->add_args();
        arg0->set_op(opConst);
        arg0->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
        arg0->mutable_subnode()->mutable_const_()->set_val(":foo");

        auto res = engine.compileAST(node, "map_invoke_test").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue val = func();
        
        assert_int_equal(42, RT_unboxInt32(val));
        release(val);
    });
}

static void test_keyword_dynamic_invoke(void **state) {
    (void)state;
    ASSERT_MEMORY_ALL_BALANCED({
        JITEngine engine;

        // AST: (let [f :foo] (f {:foo 42})) 
        // Using let ensures 'f' is a local, forcing generateDynamicInvoke
        Node node;
        node.set_op(opLet);
        auto *let = node.mutable_subnode()->mutable_let();
        
        // binding f = :foo
        auto *b = let->add_bindings();
        auto *bn = b->mutable_subnode()->mutable_binding();
        bn->set_name("f");
        auto *init = bn->mutable_init();
        init->set_op(opConst);
        init->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
        init->mutable_subnode()->mutable_const_()->set_val(":foo");

        // body = (f {:foo 42})
        auto *body = let->mutable_body();
        body->set_op(opInvoke);
        auto *inv = body->mutable_subnode()->mutable_invoke();
        inv->mutable_fn()->set_op(opLocal);
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_name("f");
        inv->mutable_fn()->mutable_subnode()->mutable_local()->set_local(localTypeLet);

        // Arg 0: {:foo 42}
        auto *argMapNode = inv->add_args();
        argMapNode->set_op(opMap);
        auto *m = argMapNode->mutable_subnode()->mutable_map();
        auto *k = m->add_keys();
        k->set_op(opConst);
        k->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeKeyword);
        k->mutable_subnode()->mutable_const_()->set_val(":foo");
        auto *v = m->add_vals();
        v->set_op(opConst);
        v->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        v->mutable_subnode()->mutable_const_()->set_val("42");

        auto res = engine.compileAST(node, "keyword_dynamic_invoke_test").get();
        RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
        RTValue val = func();
        
        assert_int_equal(42, RT_unboxInt32(val));
        release(val);
    });
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_keyword_invoke),
        cmocka_unit_test(test_keyword_dynamic_invoke),
        cmocka_unit_test(test_map_invoke),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
