#include "../jit/JITEngine.h"
#include "../state/ThreadsafeCompilerState.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

extern "C" {
#include "../bridge/InstanceCallStub.h"
#include "../runtime/RuntimeInterface.h"
#include "../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace rt;

// Helper to create a simple const node (42)
static Node create42Node() {
  Node topNode;
  topNode.set_op(opConst);
  topNode.mutable_subnode()->mutable_const_()->set_type(
      ConstNode_ConstType_constTypeNumber);
  topNode.mutable_subnode()->mutable_const_()->set_val("42");
  return topNode;
}

// 2. High Concurrency Stress Test
static void test_jit_reclamation_stress(void **state_arg) {
  (void)state_arg;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;

    Node topNode = create42Node();
    std::string moduleName = "reclamation_target";

    auto res =
        engine
            .compileAST(topNode, moduleName)
            .get();
    auto addr = res.address;
    typedef RTValue (*FnPtr)();
    std::atomic<FnPtr> funcPtr{(FnPtr)addr.getValue()};

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> callCount{0};

    const int numThreads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
      threads.emplace_back([&funcPtr, &stop, &callCount]() {
        while (!stop) {
          {
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
      auto nextAddr = engine
                          .compileAST(topNode, moduleName)
                          .get()
                          .address;
      funcPtr.store((FnPtr)nextAddr.getValue());

      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    stop = true;
    for (auto &t : threads) {
      if (t.joinable())
        t.join();
    }
  });
}

#include "bytecode.pb.h"
#include <fstream>

// Helper to load metadata from protobuf
static void setup_test_metadata(rt::ThreadsafeCompilerState &compState,
                                rt::JITEngine &engine) {
  clojure::rt::protobuf::bytecode::Programme astClasses;
  std::string path = "tests/rt-classes.cljb";
  std::fstream classesInput(path, std::ios::in | std::ios::binary);
  if (!classesInput.is_open()) {
    path = "../tests/rt-classes.cljb";
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

  try {
    auto res = engine
                   .compileAST(astClasses.nodes(0), "__classes")
                   .get()
                   .address;
    RTValue classes = res.toPtr<RTValue (*)()>()();
    compState.storeInternalClasses(classes);
  } catch (const LanguageException &e) {
    fprintf(stderr, "LanguageException during setup_test_metadata: %s\n",
            getExceptionString(e).c_str());
    throw;
  } catch (const std::exception &e) {
    fprintf(stderr, "std::exception during setup_test_metadata: %s\n",
            e.what());
    throw;
  } catch (...) {
    fprintf(stderr, "UNKNOWN EXCEPTION during setup_test_metadata\n");
    throw;
  }
}

// 3. Concurrency Under Compilation Error Test
// Bouncer declaration is now in InstanceCallStub.h

static void test_compilation_error_concurrency(void **state_arg) {
  (void)state_arg;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    setup_test_metadata(compState, engine);

    // Preparation for IC slot
    struct {
      uint64_t tag;
      void *bridge;
    } icSlotOk = {0, nullptr};
    struct {
      uint64_t tag;
      void *bridge;
    } icSlotErr = {0, nullptr};

    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};
    std::atomic<bool> start{false};

    // We need a real String object for the receiver
    RTValue strObj = RT_boxPtr(String_create("test"));
    retain(strObj);

    auto t1 = std::thread([&]() {
      while (!start)
        std::this_thread::yield();
      try {
        // Valid call: indexOf(String) is in rt-classes.edn for java.lang.String
        // InstanceCallSlowPath expects [0] = instance, [1...] = args
        // Signature (boxedMask) should be 1 if arg 0 is boxed (which it is)
        RTValue search = RT_boxPtr(String_create("e"));
        RTValue args[2] = {strObj, search};
        void *res =
            InstanceCallSlowPath(&icSlotOk, "indexOf", 1, args, 1, &engine);
        release(search);

        // res is the address of the specialized bridge
        if (res != 0) {
          successCount++;
        }
      } catch (...) {
        // Should not happen for valid call
      }
    });

    auto t2 = std::thread([&]() {
      while (!start)
        std::this_thread::yield();
      try {
        // Invalid call - non-existent method on String
        RTValue args[2] = {strObj, RT_boxInt32(42)};
        InstanceCallSlowPath(&icSlotErr, "nonExistentMethod", 1, args, 1,
                             &engine);
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
    (void)engine
        .compileAST(node, "test_after_error")
        .get()
        .address;
  });
}

void test_inheritance_bigint_tostring(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;
    setup_test_metadata(compState, engine);

    // Create a BigInt (type 13 in rt-classes.edn)
    BigInteger *bi = BigInteger_createFromInt(12345);
    RTValue biObj = RT_boxPtr(bi);

    InlineCache icSlot = {0, nullptr};
    // InstanceCallSlowPath expects [0] = instance, [1...] = args.
    // For 0-arg method toString, we need 1 element in args array.
    RTValue args[1] = {biObj};

    // This should resolve toString from java.lang.Object
    // because clojure.lang.BigInt (type 13) extends Object and doesn't have its
    // own toString in metadata.
    void *bridgePtr =
        InstanceCallSlowPath(&icSlot, "toString", 0, args, 0, &engine);
    assert_non_null(bridgePtr);

    assert_int_equal(icSlot.key, 13);

    // Call the bridge. Specialized bridge for 0-arg method takes 1 arg
    // (instance).
    typedef RTValue (*BridgeFn)(RTValue);
    BridgeFn bridgeFn = (BridgeFn)bridgePtr;
    RTValue res = bridgeFn(biObj);

    assert_true(RT_isPtr(res));
    String *s = (String *)RT_unboxPtr(res);

    // BigInteger_toString appends 'N' and creates a compound string.
    // String_c_str requires compactification.
    String *compact = String_compactify(s);
    assert_string_equal(String_c_str(compact), "12345N");
    Ptr_release(compact);
  });
}

int main(void) {
  initialise_memory();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_jit_reclamation_stress),
      cmocka_unit_test(test_compilation_error_concurrency),
      cmocka_unit_test(test_inheritance_bigint_tostring),
  };
  int result = cmocka_run_group_tests(tests, NULL, NULL);

  return result;
}
