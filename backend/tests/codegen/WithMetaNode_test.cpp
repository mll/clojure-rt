#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <iostream>

extern "C" {
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_withmeta_compile(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    Node withMetaNode;
    withMetaNode.set_op(opWithMeta);
    auto *wm = withMetaNode.mutable_subnode()->mutable_withmeta();

    // Expr: a string constant (doesn't support metadata, will return original)
    auto *expr = wm->mutable_expr();
    expr->set_op(opConst);
    expr->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
    expr->mutable_subnode()->mutable_const_()->set_val("test");

    // Meta: nil
    auto *meta = wm->mutable_meta();
    meta->set_op(opConst);
    meta->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNil);
    meta->mutable_subnode()->mutable_const_()->set_val("nil");

    auto res = engine.compileAST(withMetaNode, "__test_withmeta").get().address;

    RTValue result = res.toPtr<RTValue (*)()>()();
    assert_true(RT_isPtr(result));
    
    // We expect the original string back
    String *s = (String *)RT_unboxPtr(result);
    assert_string_equal("test", String_c_str(s));
    
    release(result);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_withmeta_compile),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
