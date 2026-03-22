#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

extern "C" {
RTValue mock_add_bigint(RTValue a, RTValue b) {
  release(a);
  release(b);
  return RT_boxPtr(BigInteger_createFromInt(42));
}
}

static void test_do_statement_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;

    // (do (+ 1N 2N) 42)
    Node root;
    root.set_op(opDo);
    auto *do_ = root.mutable_subnode()->mutable_do_();

    auto *stmt = do_->add_statements();
    stmt->set_op(opStaticCall);
    auto *sc = stmt->mutable_subnode()->mutable_staticcall();
    sc->set_class_("clojure.lang.Numbers");
    sc->set_method("add");
    
    auto *a1 = sc->add_args();
    a1->set_op(opConst);
    a1->mutable_subnode()->mutable_const_()->set_val("1");
    a1->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    a1->set_tag("clojure.lang.BigInt");

    auto *a2 = sc->add_args();
    a2->set_op(opConst);
    a2->mutable_subnode()->mutable_const_()->set_val("2");
    a2->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    a2->set_tag("clojure.lang.BigInt");

    auto *ret = do_->mutable_ret();
    ret->set_op(opConst);
    ret->mutable_subnode()->mutable_const_()->set_val("42");
    ret->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
    ret->set_tag("long");

    // Setup clojure.lang.Numbers/add metadata
    auto desc = std::make_unique<rt::ClassDescription>();
    rt::IntrinsicDescription numAdd;
    numAdd.type = rt::CallType::Call;
    numAdd.symbol = "mock_add_bigint"; 
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.argTypes.push_back(rt::ObjectTypeSet::all());
    numAdd.returnType = rt::ObjectTypeSet::all();
    desc->staticFns["add"].push_back(numAdd);

    String *numName = String_createDynamicStr("clojure.lang.Numbers");
    Ptr_retain(numName);
    ::Class *numCls = Class_create(numName, numName, 0, nullptr);
    numCls->compilerExtension = desc.release();
    numCls->compilerExtensionDestructor = rt::delete_class_description;
    compState.classRegistry.registerObject("clojure.lang.Numbers", numCls);

    cout << "=== Do Statement Leak Test (Should NOT leak) ===" << endl;
    try {
      rt::JITEngine engine(compState);
      auto res = engine.compileAST(root, "__test_do_leak", llvm::OptimizationLevel::O0, true).get().address;
      
      RTValue val = res.toPtr<RTValue (*)()>()();
      assert_int_equal(42, RT_unboxInt32(val));
    } catch (const std::exception &e) {
      cout << "Caught exception: " << e.what() << endl;
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_do_statement_leak),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
