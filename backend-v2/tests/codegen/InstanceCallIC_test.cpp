#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
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

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Mock implementations for instance methods
extern "C" {
int32_t mock_TypeA_foo(PersistentVector *instance, int32_t x) {
  Ptr_release(instance);
  return 1000 + x;
}
int32_t mock_TypeB_foo(PersistentList *instance, int32_t x) {
  Ptr_release(instance);
  return 2000 + x;
}
}

static void setup_ic_test_metadata(rt::ThreadsafeCompilerState &compState) {
  // TypeA: foo -> 1000 + x
  {
    String *nameStr = String_create("TypeA");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    ClassDescription *ext = new ClassDescription();
    ext->name = "TypeA";
    ext->type = ObjectTypeSet((objectType)persistentVectorType);

    IntrinsicDescription foo;
    foo.symbol = "mock_TypeA_foo";
    foo.type = CallType::Call;
    foo.argTypes.push_back(ObjectTypeSet((objectType)persistentVectorType));
    foo.argTypes.push_back(ObjectTypeSet((objectType)integerType, false));
    foo.returnType = ObjectTypeSet((objectType)integerType, false);
    ext->instanceFns["foo"].push_back(foo);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(cls, (int32_t)persistentVectorType);
  }

  // TypeB: foo -> 2000 + x
  {
    String *nameStr = String_create("TypeB");
    Ptr_retain(nameStr);
    Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
    ClassDescription *ext = new ClassDescription();
    ext->name = "TypeB";
    ext->type = ObjectTypeSet((objectType)persistentListType);

    IntrinsicDescription foo;
    foo.symbol = "mock_TypeB_foo";
    foo.type = CallType::Call;
    foo.argTypes.push_back(ObjectTypeSet((objectType)persistentListType));
    foo.argTypes.push_back(ObjectTypeSet((objectType)integerType, false));
    foo.returnType = ObjectTypeSet((objectType)integerType, false);
    ext->instanceFns["foo"].push_back(foo);

    cls->compilerExtension = ext;
    cls->compilerExtensionDestructor = delete_class_description;
    compState.classRegistry.registerObject(cls, (int32_t)persistentListType);
  }
}

static void test_instance_call_ic_hit_miss(void **state) {
  (void)state;

  // Pre-populate shared constants outside the balance check
  PersistentVector_initialise();
  PersistentList_empty();

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;
    setup_ic_test_metadata(compState);
    JITEngine engine(compState);

    // Register a Var "user/my-var" to hold our instance
    RTValue varKeyword = Keyword_create(String_create("user/my-var"));
    Var *myVar = Var_create(varKeyword);
    compState.varRegistry.registerObject("user/my-var", myVar);

    // AST: (.foo @my-var 10)
    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("foo");

    // Instance: (deref my-var) -> untagged/dynamic
    auto *inst = ic->mutable_instance();
    inst->set_op(opVar);
    inst->mutable_subnode()->mutable_var()->set_var("#'user/my-var");

    // Arg: 10
    auto *arg = ic->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("10");
    arg->set_tag("long"); // Tagged as int32 unboxed

    try {
      // Compile once
      auto resF = engine
                      .compileAST(callNode, "__test_ic_dynamic",
                                  llvm::OptimizationLevel::O0, true)
                      .get();
      auto fn = resF.toPtr<RTValue (*)()>();

      // --- Phase 1: Call with TypeA (Vector) ---
      RTValue vec = RT_boxPtr(PersistentVector_create());
      Ptr_retain(myVar);
      Var_bindRoot(myVar, vec);

      RTValue r1 = fn();
      assert_int_equal(1010, RT_unboxInt32(r1));
      release(r1);

      // --- Phase 2: Call again with TypeA (Cache Hit) ---
      RTValue r2 = fn();
      assert_int_equal(1010, RT_unboxInt32(r2));
      release(r2);

      // --- Phase 3: Call with TypeB (List) -> Cache Miss ---
      RTValue list = RT_boxPtr(PersistentList_empty());
      Ptr_retain(myVar);
      Var_bindRoot(myVar, list);

      RTValue r3 = fn();
      assert_int_equal(2010, RT_unboxInt32(r3));
      release(r3);

      // --- Phase 4: Call again with TypeB (New Cache Hit) ---
      RTValue r4 = fn();
      assert_int_equal(2010, RT_unboxInt32(r4));
      release(r4);

      // Cleanup
      Ptr_retain(myVar);
      Var_bindRoot(myVar, RT_boxNil());

    } catch (const std::exception &e) {
      fprintf(stderr, "Exception in IC test: %s\n", e.what());
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();
  PersistentVector_initialise();
  PersistentList_empty();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_instance_call_ic_hit_miss),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
