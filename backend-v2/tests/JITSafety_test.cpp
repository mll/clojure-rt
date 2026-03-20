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

// Helper to create a simple const node (42)
static Node create42Node() {
    Node topNode;
    topNode.set_op(opConst);
    topNode.mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    topNode.mutable_subnode()->mutable_const_()->set_val("42");
    return topNode;
}

// 1. Deterministic Multi-Stage Reclamation Test
static void test_two_stage_reclamation(void **state_arg) {
    (void)state_arg;
    
    ThreadsafeCompilerState compState;
    JITEngine engine(compState);

    Node topNode = create42Node();
    std::string moduleName = "safety_test_module";
    
    // Initial compilation
    engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    
    assert_int_equal(engine.getLimboCount(), 0);
    assert_int_equal(engine.getZombieCount(), 0);

    // Someone enters a safety section and stays there
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

    // Recompile - the old module should go to Limbo
    engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    
    assert_int_equal(engine.getLimboCount(), 1);
    assert_int_equal(engine.getZombieCount(), 0);

    // Commit - moves from Limbo to Zombie
    engine.commit(moduleName);
    assert_int_equal(engine.getLimboCount(), 0);
    assert_int_equal(engine.getZombieCount(), 1);

    // Sweep - should NOT remove because holder thread is active
    engine.sweep();
    assert_int_equal(engine.getZombieCount(), 1);

    // Release holder thread and wait
    releaseSafety = true;
    holder.join();

    // Sweep again - now it should be gone
    engine.sweep();
    assert_int_equal(engine.getZombieCount(), 0);
}

// 2. High Concurrency Stress Test
static void test_jit_reclamation_stress(void **state_arg) {
    (void)state_arg;
    
    ThreadsafeCompilerState compState;
    JITEngine engine(compState);

    Node topNode = create42Node();
    std::string moduleName = "reclamation_target";
    
    auto addr = engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
    typedef RTValue (*FnPtr)();
    std::atomic<FnPtr> funcPtr{(FnPtr)addr.getValue()};

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> callCount{0};

    const int numThreads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&engine, &funcPtr, &stop, &callCount]() {
            while (!stop) {
                {
                    JITEngine::SafetySection safety(engine);
                    FnPtr f = funcPtr.load();
                    RTValue res = f();
                    if (RT_unboxInt32(res) != 42) {
                        abort();
                    }
                    callCount++;
                }
                std::this_thread::yield();
            }
        });
    }

    for (int i = 0; i < 50; i++) {
        auto nextAddr = engine.compileAST(topNode, moduleName, llvm::OptimizationLevel::O0, false).get();
        funcPtr.store((FnPtr)nextAddr.getValue());
        engine.commit(moduleName);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    stop = true;
    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }
}

#include "bytecode.pb.h"
#include <fstream>

// Helper to load metadata from protobuf
static void setup_test_metadata(rt::ThreadsafeCompilerState &compState, rt::JITEngine &engine) {
    clojure::rt::protobuf::bytecode::Programme astClasses;
    std::string path = "tests/rt-classes.cljb";
    std::fstream classesInput(path, std::ios::in | std::ios::binary);
    if (!classesInput.is_open()) {
        path = "backend-v2/tests/rt-classes.cljb";
        classesInput.open(path, std::ios::in | std::ios::binary);
    }
    
    if (classesInput.is_open()) {
        if (!astClasses.ParseFromIstream(&classesInput)) {
            fprintf(stderr, "Failed to parse bytecode from %s\n", path.c_str());
            return;
        }
    } else {
        fprintf(stderr, "Could not open rt-classes.cljb\n");
        return;
    }

    auto res = engine.compileAST(astClasses.nodes(0), "__classes", llvm::OptimizationLevel::O0, false).get();
    RTValue classes = res.toPtr<RTValue (*)()>()();
    compState.storeInternalClasses(classes);
}

// 3. Concurrency Under Compilation Error Test
// External declaration of the bouncer we want to test
extern "C" RTValue InstanceCallSlowPath(void* icSlot, const char* methodName, int argCount, RTValue* args, uint64_t signature, void* jitEnginePtr);

static void test_compilation_error_concurrency(void **state_arg) {
    (void)state_arg;
    
    rt::ThreadsafeCompilerState compState;
    JITEngine engine(compState);
    setup_test_metadata(compState, engine);
    
    // Preparation for IC slot
    struct { uint64_t tag; void* bridge; } icSlotOk = { 0, nullptr };
    struct { uint64_t tag; void* bridge; } icSlotErr = { 0, nullptr };
    
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};
    std::atomic<bool> start{false};
    
    // We need a real String object for the receiver
    RTValue strObj = RT_boxPtr(String_create("test"));
    retain(strObj);

    auto t1 = std::thread([&]() {
        while(!start) std::this_thread::yield();
        try {
            // Valid call: indexOf(String) is in rt-classes.edn for java.lang.String
            // InstanceCallSlowPath expects [0] = instance, [1...] = args
            // Signature (boxedMask) should be 1 if arg 0 is boxed (which it is)
            RTValue search = RT_boxPtr(String_create("e"));
            RTValue args[2] = { strObj, search };
            RTValue res = InstanceCallSlowPath(&icSlotOk, "indexOf", 1, args, 1, &engine);
            
            // res is the address of the specialized bridge
            if (res != 0) {
                successCount++;
            }
        } catch (...) {
            // Should not happen for valid call
        }
    });

    auto t2 = std::thread([&]() {
        while(!start) std::this_thread::yield();
        try {
            // Invalid call - non-existent method on String
            RTValue args[2] = { strObj, RT_boxInt32(42) };
            InstanceCallSlowPath(&icSlotErr, "nonExistentMethod", 1, args, 1, &engine);
        } catch (...) {
            // We expect an exception here!
            failureCount++;
        }
    });

    start = true;
    t1.join();
    t2.join();

    assert_int_equal(successCount, 1);
    assert_int_equal(failureCount, 1);
    
    release(strObj);

    // Verify system stability
    Node node = create42Node();
    engine.compileAST(node, "test_after_error", llvm::OptimizationLevel::O0, false).get();
    engine.commit("test_after_error");
    engine.sweep(); 
}

int main(void) {
    initialise_memory();
    RuntimeInterface_initialise();
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_two_stage_reclamation),
        cmocka_unit_test(test_jit_reclamation_stress),
        cmocka_unit_test(test_compilation_error_concurrency),
    };
    int result = cmocka_run_group_tests(tests, NULL, NULL);
    RuntimeInterface_cleanup();
    return result;
}
