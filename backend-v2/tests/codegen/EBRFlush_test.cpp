#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Object.h"
#include "../../runtime/Ebr.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/defines.h"

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_ebr_flush_on_threshold(void **state) {
    (void)state;
    rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
    
    // (fn [] (do 1 2 3 ... 21))
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(0);
    auto *body = mn->mutable_body();
    body->set_op(opDo);
    auto *doNode = body->mutable_subnode()->mutable_do_();
    
    // Add EBR_FLUSH_NODE_THRESHOLD + 1 nodes
    for (int i = 0; i < EBR_FLUSH_NODE_THRESHOLD + 1; ++i) {
        auto *stmt = doNode->add_statements();
        stmt->set_op(opConst);
        stmt->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
        stmt->mutable_subnode()->mutable_const_()->set_val("1");
    }
    auto *ret = doNode->mutable_ret();
    ret->set_op(opConst);
    ret->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    ret->mutable_subnode()->mutable_const_()->set_val("1");

    // Compile. We don't even need to run it, just check the IR.
    auto res = engine.compileAST(fnNode, "test_ebr_flush_on_threshold").get();
    
    // Check if Ebr_flush_critical is present in the IR
    assert_non_null(strstr(res.optimizedIR.c_str(), "Ebr_flush_critical"));
}

static void test_ebr_flush_on_invoke(void **state) {
    (void)state;
    rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
    
    // (fn [] (some-fn)) - Small node count but contains an invoke
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(0);
    auto *body = mn->mutable_body();
    
    body->set_op(opInvoke); 
    auto *inv = body->mutable_subnode()->mutable_invoke();
    auto *innerFn = inv->mutable_fn();
    innerFn->set_op(opConst);
    innerFn->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    innerFn->mutable_subnode()->mutable_const_()->set_val("0"); 

    // Compile
    auto res = engine.compileAST(fnNode, "test_ebr_flush_on_invoke").get();
    
    // Check if Ebr_flush_critical is present in the IR
    assert_non_null(strstr(res.optimizedIR.c_str(), "Ebr_flush_critical"));
}

static void test_no_ebr_flush_for_simple(void **state) {
    (void)state;
    rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
    
    // (fn [] 1)
    Node fnNode;
    fnNode.set_op(opFn);
    auto *fn = fnNode.mutable_subnode()->mutable_fn();
    auto *m = fn->add_methods();
    auto *mn = m->mutable_subnode()->mutable_fnmethod();
    mn->set_fixedarity(0);
    auto *body = mn->mutable_body();
    body->set_op(opConst);
    body->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    body->mutable_subnode()->mutable_const_()->set_val("1");

    // Compile
    auto res = engine.compileAST(fnNode, "test_no_ebr_flush_for_simple").get();
    
    // Check if Ebr_flush_critical is absent in the IR
    assert_null(strstr(res.optimizedIR.c_str(), "Ebr_flush_critical"));
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ebr_flush_on_threshold),
        cmocka_unit_test(test_ebr_flush_on_invoke),
        cmocka_unit_test(test_no_ebr_flush_for_simple),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
