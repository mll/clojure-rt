#include "../../bridge/Exceptions.h"
#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../tools/EdnParser.h"

#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include "runtime/Ebr.h"
#include <iostream>

#include "../../runtime/ExecutionContext.h"
#include "../../runtime/Keyword.h"
#include "../../runtime/Numbers.h"
#include "../../runtime/PersistentArrayMap.h"
#include "../../runtime/String.h"
#include "../../runtime/Var.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

typedef RTValue (*JitFn)(ExecutionContext *__attribute__((swift_context)))
    __attribute__((swiftcall));

static void test_context_propagation_in_jit(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    RTValue varKeyword = Keyword_create(String_create("user/test-var"));

    Var *myVar = Var_create(varKeyword);
    Var_setDynamic(myVar, true);

    Ptr_retain(myVar);
    Var_bindRoot(myVar, RT_boxInt32(1));

    String *varClassNameString = String_create("clojure.lang.Var");
    Ptr_retain(varClassNameString);
    ::Class *varCls =
        Class_create(varClassNameString, varClassNameString, 0, nullptr);
    varCls->registerId = varType;

    rt::ClassDescription *ext = new rt::ClassDescription();
    ext->name = "clojure.lang.Var";
    ext->type = ObjectTypeSet(varType);

    rt::IntrinsicDescription derefDesc;
    derefDesc.symbol = "Var_deref";
    derefDesc.type = rt::CallType::Call;
    derefDesc.argTypes.push_back(ObjectTypeSet(varType));
    derefDesc.returnType = ObjectTypeSet::all();
    derefDesc.passBindingContext = true;

    ext->implements["clojure.lang.IDeref"]["deref"].push_back(derefDesc);

    varCls->compilerExtension = ext;
    varCls->compilerExtensionDestructor = rt::delete_class_description;

    Ptr_retain(varCls);
    compState.classRegistry.registerObject("clojure.lang.Var", varCls);
    compState.classRegistry.registerObject(varCls, varType);

    compState.varRegistry.registerObject("user/test-var", myVar);

    Node callNode;
    callNode.set_op(opInstanceCall);
    auto *ic = callNode.mutable_subnode()->mutable_instancecall();
    ic->set_method("deref");
    auto *inst = ic->mutable_instance();
    inst->set_op(opTheVar);
    inst->mutable_subnode()->mutable_thevar()->set_var("#'user/test-var");

    try {
      // We use compileASTWithContext specifically for this test
      auto res = engine.compileASTWithContext(callNode, "test_context_prop")
                     .get()
                     .address;
      JitFn fn = (JitFn)res.toPtr<void *>();

      RTValue val100 = RT_boxInt32(100);
      PersistentArrayMap *m =
          PersistentArrayMap_createMany(1, varKeyword, val100);
      ExecutionContext *ctx = ExecutionContext_create(RT_boxPtr(m));

      RTValue result = fn(ctx);

      assert_true(RT_isInt32(result));
      assert_int_equal(RT_unboxInt32(result), 100);

      release(result);
      Ptr_release(ctx);
    } catch (const std::exception &e) {
      printf("EXCEPTION: %s\n", e.what());
      fflush(stdout);
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_context_propagation_in_jit),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
