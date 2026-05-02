#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/Function.h"
#include "../../runtime/Object.h"
#include "../../runtime/Var.h"
#include "runtime/ObjectProto.h"
#include "runtime/PersistentArrayMap.h"
#include "runtime/RTValue.h"
#include <atomic>
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
}

using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void test_const_metadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // AST: ^:foo test-symbol
    Node constNode;
    constNode.set_op(opConst);
    auto *c = constNode.mutable_subnode()->mutable_const_();
    c->set_type(ConstNode_ConstType_constTypeSymbol);
    c->set_val("test-symbol");

    // Metadata: {:foo true}
    auto *meta = c->mutable_meta();
    meta->set_op(opMap);
    auto *m = meta->mutable_subnode()->mutable_map();

    auto *key = m->add_keys();
    key->set_op(opConst);
    key->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeKeyword);
    key->mutable_subnode()->mutable_const_()->set_val(":foo");

    auto *val = m->add_vals();
    val->set_op(opConst);
    val->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeBool);
    val->mutable_subnode()->mutable_const_()->set_val("true");

    auto res = engine.compileAST(constNode, "const_metadata_test").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue result = func();

    assert_true(getType(result) == symbolType);
    retain(result);
    String *rts = Symbol_getName((Symbol *)RT_unboxPtr(result));
    assert_string_equal("test-symbol", String_c_str(rts));
    Ptr_release(rts);

    RTValue metadata = RT_meta(result); // Consumes result
    assert_true(getType(metadata) == persistentArrayMapType);

    RTValue fooKey = Keyword_create(String_create("foo"));
    RTValue fooVal =
        PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(metadata),
                               fooKey); // Consumes metadata and fooKey
    assert_true(getType(fooVal) == booleanType);
    assert_true(RT_unboxBool(fooVal));

    release(fooVal);
  });
}

static void test_def_metadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;

    // AST: (def ^:bar x 100)
    Node defNode;
    defNode.set_op(opDef);
    auto *d = defNode.mutable_subnode()->mutable_def();
    d->set_var("#'user/test-metadata-var");

    auto *init = d->mutable_init();
    init->set_op(opConst);
    init->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    init->mutable_subnode()->mutable_const_()->set_val("100");

    // Metadata: {:bar 123}
    auto *meta = d->mutable_meta();
    meta->set_op(opMap);
    auto *m = meta->mutable_subnode()->mutable_map();

    auto *key = m->add_keys();
    key->set_op(opConst);
    key->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeKeyword);
    key->mutable_subnode()->mutable_const_()->set_val(":bar");

    auto *val = m->add_vals();
    val->set_op(opConst);
    val->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    val->mutable_subnode()->mutable_const_()->set_val("123");

    auto res = engine.compileAST(defNode, "def_metadata_test").get();
    RTValue (*func)() = res.address.toPtr<RTValue (*)()>();
    RTValue varObj = func(); // Returns the Var

    Var *v = (Var *)RT_unboxPtr(varObj);

    RTValue metadata = Var_getMeta(v); // Consumes varObj (v)
    assert_true(getType(metadata) == persistentArrayMapType);

    RTValue barKey = Keyword_create(String_create("bar"));
    RTValue barVal =
        PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(metadata),
                               barKey); // Consumes metadata and barKey
    assert_int_equal(123, RT_unboxInt32(barVal));

    release(barVal);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_const_metadata),
      cmocka_unit_test(test_def_metadata),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
