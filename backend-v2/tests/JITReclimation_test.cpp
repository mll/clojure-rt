#include "../RuntimeHeaders.h"
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

static void test_jit_module_reclamation(void **state_arg) {
    (void)state_arg;
    printf("--- Starting JIT Module Reclamation Test ---\n");

    ThreadsafeCompilerState compState;
    JITEngine engine(compState);

    // Simple AST: 42
    Node topNode;
    topNode.set_op(opConst);
    topNode.mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    topNode.mutable_subnode()->mutable_const_()->set_val("42");

    std::string moduleName = "reclamation_target";
    
    // Initial compilation
    auto addr = engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    typedef RTValue (*FnPtr)();
    std::atomic<FnPtr> funcPtr{(FnPtr)addr.getValue()};

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> callCount{0};

    // Spawn caller threads
    const int numThreads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&engine, &funcPtr, &stop, &callCount]() {
            while (!stop) {
                {
                    JITEngine::SafetySection safety(engine);
                    FnPtr f = funcPtr.load();
                    RTValue res = f();
                    // Just to be sure it's doing something
                    if (RT_unboxInt32(res) != 42) {
                        printf("Error: expected 42, got %lld\n", (long long)RT_unboxInt32(res));
                        abort();
                    }
                    callCount++;
                }
                // Small yield to allow switcher to breathe
                std::this_thread::yield();
            }
        });
    }

    // Main thread: repeatedly recompile to trigger reclamation
    printf("Triggering 100 recompilations...\n");
    for (int i = 0; i < 100; i++) {
        auto nextAddr = engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
        funcPtr.store((FnPtr)nextAddr.getValue());
        engine.commit(moduleName);
        if (i % 20 == 0) {
            printf("Progress: %d/100 recompilations, total calls: %llu\n", i, (unsigned long long)callCount.load());
        }
        // Small delay to ensure some calls happen per version
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    printf("Finalizing...\n");
    stop = true;
    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }

    printf("Total calls made: %llu\n", (unsigned long long)callCount.load());
    printf("--- JIT Module Reclamation Test Finished ---\n");
}

int main(void) {
    initialise_memory();
    // We don't need full runtime init for this simple test, but it doesn't hurt.
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_jit_module_reclamation),
    };

    int result = cmocka_run_group_tests(tests, NULL, NULL);
    
    RuntimeInterface_cleanup();
    return result;
}
