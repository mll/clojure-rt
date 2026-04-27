#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include "runtime/Ebr.h"
#include <fstream>
#include <iostream>

extern "C" {
#include "../../runtime/Keyword.h"
#include "../../runtime/PersistentList.h"
#include "../../runtime/PersistentVector.h"
#include "../../runtime/String.h"
#include "../../runtime/Var.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include <atomic>
#include <vector>

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

extern "C" _Atomic(uword_t) globalMethodICEpoch;

// Mock implementations
extern "C" {
int32_t mock_P1_m1_v1(PersistentVector *instance, int32_t x) {
  Ptr_release(instance);
  return 100 + x;
}
int32_t mock_P1_m1_v2(PersistentVector *instance, int32_t x) {
  Ptr_release(instance);
  return 200 + x;
}
}

static void test_protocol_instance_call_lookup_and_invalidation(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // 1. Define Protocol P1
    {
      String *nameStr = String_create("P1");
      String *classNameStr = String_create("P1");
      Class *cls = Class_createProtocol(nameStr, classNameStr, 0, nullptr);
      ClassDescription *ext = new ClassDescription();
      ext->name = "P1";
      ext->isProtocol = true;
      ext->type = ObjectTypeSet(classType); // Protocols use classType for now
      cls->compilerExtension = ext;
      cls->compilerExtensionDestructor = delete_class_description;
      compState.protocolRegistry.registerObject("P1", cls);
    }

    // 2. Define Class C1 implementing P1
    auto setupC1 = [&](const string& symbol) {
      String *nameStr = String_create("C1");
      String *classNameStr = String_create("C1");
      Class *cls = Class_create(nameStr, classNameStr, 0, nullptr);
      ClassDescription *ext = new ClassDescription();
      ext->name = "C1";
      ext->type = ObjectTypeSet((objectType)persistentVectorType);

      IntrinsicDescription m1;
      m1.symbol = symbol;
      m1.type = CallType::Call;
      m1.argTypes.push_back(ObjectTypeSet((objectType)persistentVectorType));
      m1.argTypes.push_back(ObjectTypeSet((objectType)integerType, false));
      m1.returnType = ObjectTypeSet((objectType)integerType, false);
      
      // Add implementation under protocol P1
      ext->implements["P1"]["m1"].push_back(m1);

      cls->compilerExtension = ext;
      cls->compilerExtensionDestructor = delete_class_description;
      compState.classRegistry.registerObject("C1", cls);
      Ptr_retain(cls); // Registry doesn't retain on register, and we have two registries
      compState.classRegistry.registerObject(cls, (int32_t)persistentVectorType);
      atomic_fetch_add_explicit(&globalMethodICEpoch, 1, memory_order_release);
    };

    setupC1("mock_P1_m1_v1");

    // Register Var user/x
    RTValue varKeyword = Keyword_create(String_create("user/x"));
    Var *myVar = Var_create(varKeyword);
    compState.varRegistry.registerObject("user/x", myVar);

    // AST: (.m1 @user/x 10)
    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("m1");
    auto *inst = ic->mutable_instance();
    inst->set_op(opVar);
    inst->mutable_subnode()->mutable_var()->set_var("#'user/x");
    auto *arg = ic->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("10");
    arg->set_tag("long");

    try {
      auto resF = engine.compileAST(callNode, "test_proto_call").get().address;
      auto fn = resF.toPtr<RTValue (*)()>();

      // --- Phase 1: Call v1 ---
      RTValue vec = RT_boxPtr(PersistentVector_create());
      Ptr_retain(myVar);
      Var_bindRoot(myVar, vec);

      RTValue r1 = fn();
      assert_int_equal(110, RT_unboxInt32(r1));
      release(r1);

      // --- Phase 2: Warm IC ---
      RTValue r2 = fn();
      assert_int_equal(110, RT_unboxInt32(r2));
      release(r2);

      // --- Phase 3: Redefine C1 (v2) ---
      // This should increment globalMethodICEpoch
      setupC1("mock_P1_m1_v2");

      // --- Phase 4: Call again (should pick up v2 because IC was invalidated) ---
      RTValue r3 = fn();
      assert_int_equal(210, RT_unboxInt32(r3));
      release(r3);

      // Cleanup
      Ptr_retain(myVar);
      Var_bindRoot(myVar, RT_boxNil());
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception: %s\n", e.what());
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_protocol_instance_call_lookup_and_invalidation),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
