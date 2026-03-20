#include "../jit/JITEngine.h"
#include "../state/ThreadsafeCompilerState.h"
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>

extern "C" {
#include "../runtime/RuntimeInterface.h"
#include "../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;

static void test_two_stage_reclamation(void **state_arg) {
    (void)state_arg;
    
    ThreadsafeCompilerState compState;
    JITEngine engine(compState);

    // Simple AST: 42
    Node topNode;
    topNode.set_op(opConst);
    topNode.mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    topNode.mutable_subnode()->mutable_const_()->set_val("42");

    std::string moduleName = "safety_test_module";
    
    // 1. Initial compilation
    engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    
    assert_int_equal(engine.getLimboCount(), 0);
    assert_int_equal(engine.getZombieCount(), 0);

    // 2. Someone enters a safety section and stays there
    std::atomic<bool> inSafety{false};
    std::atomic<bool> releaseSafety{false};
    std::thread holder([&engine, &inSafety, &releaseSafety]() {
        {
            JITEngine::SafetySection safety(engine);
            inSafety = true;
            while (!releaseSafety) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });

    while (!inSafety) std::this_thread::yield();

    // 3. Recompile - the old module should go to Limbo
    engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    
    // In Limbo because not committed yet
    assert_int_equal(engine.getLimboCount(), 1);
    assert_int_equal(engine.getZombieCount(), 0);

    // 4. Commit - moves from Limbo to Zombie
    engine.commit(moduleName);
    assert_int_equal(engine.getLimboCount(), 0);
    assert_int_equal(engine.getZombieCount(), 1);

    // 5. Sweep - should NOT remove because holder thread is active
    engine.sweep();
    assert_int_equal(engine.getZombieCount(), 1);

    // 6. Release holder thread and wait
    releaseSafety = true;
    holder.join();

    // 7. Sweep again - now it should be gone
    engine.sweep();
    assert_int_equal(engine.getZombieCount(), 0);
}

int main(void) {
    initialise_memory();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_two_stage_reclamation),
    };
    int result = cmocka_run_group_tests(tests, NULL, NULL);
    RuntimeInterface_cleanup();
    return result;
}
