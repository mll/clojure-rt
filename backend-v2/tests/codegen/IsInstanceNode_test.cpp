#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include "runtime/ObjectProto.h"
#include <memory>
#include <vector>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

extern "C" {
RTValue MyClass_create() {
  Object *obj = (Object *)malloc(sizeof(Object));
  Object_create(obj, (objectType)1000);
  return (RTValue)(uintptr_t)obj;
}

RTValue someMethod() { return RT_boxNil(); }
}

static void prepareEmptyClass(uint32_t type, ThreadsafeCompilerState &compState,
                              const char *name) {
  String *nameStr = String_create(name);
  Ptr_retain(nameStr);
  ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
  cls->registerId = type;

  ClassDescription *ext = new ClassDescription();
  ext->name = name;
  ext->type = ObjectTypeSet(type);
  cls->compilerExtension = ext;
  cls->compilerExtensionDestructor = delete_class_description;

  compState.classRegistry.registerObject(name, cls);
  Ptr_retain(cls);
  compState.classRegistry.registerObject(cls, type);
}

static void setup_test_metadata(rt::ThreadsafeCompilerState &compState) {
  prepareEmptyClass(nilType, compState, "Nil");
  prepareEmptyClass(integerType, compState, "Integer");
  // Register MyClass with registerId = 1000
  String *nameStr = String_create("MyClass");
  Ptr_retain(nameStr);
  ::Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
  cls->registerId = 1000;

  ClassDescription *ext = new ClassDescription();
  ext->name = "MyClass";
  ext->type = ObjectTypeSet((objectType)1000);

  IntrinsicDescription id;
  id.symbol = "someMethod";
  id.type = CallType::Call;
  id.returnType = ObjectTypeSet::dynamicType();
  id.argTypes = {};
  ext->staticFns["someMethod"] = {id};

  cls->compilerExtension = ext;
  cls->compilerExtensionDestructor = delete_class_description;

  // Add a constructor
  rt::IntrinsicDescription ctor;
  ctor.symbol = "MyClass_create";
  ctor.type = rt::CallType::Call;
  ctor.returnType = rt::ObjectTypeSet((objectType)1000);
  ctor.argTypes = {};
  ext->constructors.push_back(ctor);

  compState.classRegistry.registerObject("MyClass", cls);
  Ptr_retain(cls);
  compState.classRegistry.registerObject(cls, 1000);
}

static void test_is_instance_compile_time_true(void **state) {
  (void)state;
  Node node;
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    node.set_op(opIsInstance);
    auto *is = node.mutable_subnode()->mutable_isinstance();
    is->set_class_("MyClass");

    auto *target = is->mutable_target();
    target->set_op(opNew);
    auto *nn = target->mutable_subnode()->mutable_new_();
    auto *clsNode = nn->mutable_class_();
    clsNode->set_op(opConst);
    clsNode->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    clsNode->mutable_subnode()->mutable_const_()->set_val("MyClass");

    auto res = engine
                   .compileAST(node, "test_is_instance_compile_time_true",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;

    RTValue result = resPtrToValue(res);
    assert_true(RT_isBool(result));
    assert_true(RT_unboxBool(result));
    release(result);
  });
}

static void test_is_instance_compile_time_false(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    Node node;
    node.set_op(opIsInstance);
    auto *is = node.mutable_subnode()->mutable_isinstance();
    is->set_class_("MyClass");

    auto *target = is->mutable_target();
    target->set_op(opConst);
    target->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    target->mutable_subnode()->mutable_const_()->set_val("42");
    target->set_tag("long");

    auto res = engine
                   .compileAST(node, "test_is_instance_compile_time_false",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;

    RTValue result = resPtrToValue(res);
    assert_true(RT_isBool(result));
    assert_false(RT_unboxBool(result));
    release(result);
  });
}

static void test_is_instance_runtime(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    Node node;
    node.set_op(opIsInstance);
    auto *is = node.mutable_subnode()->mutable_isinstance();
    is->set_class_("MyClass");

    auto *target = is->mutable_target();
    target->set_op(opLocal);
    target->mutable_subnode()->mutable_local()->set_name("dynamic-val");

    Node letNode;
    letNode.set_op(opLet);
    auto *ln = letNode.mutable_subnode()->mutable_let();
    auto *bn = ln->add_bindings();
    bn->set_op(opBinding);
    auto *b = bn->mutable_subnode()->mutable_binding();
    b->set_name("dynamic-val");
    auto *init = b->mutable_init();
    init->set_op(opConst);
    init->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNil);
    ln->mutable_body()->CopyFrom(node);

    auto res = engine
                   .compileAST(letNode, "test_is_instance_runtime",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;

    RTValue result = resPtrToValue(res);
    assert_true(RT_isBool(result));
    assert_false(RT_unboxBool(result));
    release(result);
  });
}

static void test_is_instance_class_prefix(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    Node node;
    node.set_op(opIsInstance);
    auto *is = node.mutable_subnode()->mutable_isinstance();
    is->set_class_("class MyClass");

    auto *target = is->mutable_target();
    target->set_op(opNew);
    auto *nn = target->mutable_subnode()->mutable_new_();
    auto *clsNode = nn->mutable_class_();
    clsNode->set_op(opConst);
    clsNode->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    clsNode->mutable_subnode()->mutable_const_()->set_val("MyClass");

    auto res = engine
                   .compileAST(node, "test_is_instance_class_prefix",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;

    RTValue result = resPtrToValue(res);
    assert_true(RT_isBool(result));
    assert_true(RT_unboxBool(result));
    release(result);
  });
}

static void test_is_instance_any_type_regression(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    Node letNode;
    letNode.set_op(opLet);
    auto *ln = letNode.mutable_subnode()->mutable_let();
    auto *bn = ln->add_bindings();
    bn->set_op(opBinding);
    auto *b = bn->mutable_subnode()->mutable_binding();
    b->set_name("dynamic-val");
    auto *init = b->mutable_init();
    init->set_op(opStaticCall);
    auto *sc = init->mutable_subnode()->mutable_staticcall();
    sc->set_class_("MyClass");
    sc->set_method("someMethod");

    auto *body = ln->mutable_body();
    body->set_op(opIsInstance);
    auto *is = body->mutable_subnode()->mutable_isinstance();
    is->set_class_("MyClass");
    auto *target = is->mutable_target();
    target->set_op(opLocal);
    target->mutable_subnode()->mutable_local()->set_name("dynamic-val");

    auto res = engine
                   .compileAST(letNode, "test_is_instance_any_type_regression",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;

    RTValue result = resPtrToValue(res);
    assert_true(RT_isBool(result));
    assert_false(RT_unboxBool(result));
    release(result);
  });
}

static void test_is_instance_refcounted_local(void **state) {
  (void)state;
  // (let [x 123N] (instance? MyClass x))
  ASSERT_MEMORY_ALL_BALANCED({
    ThreadsafeCompilerState compilerState;
    setup_test_metadata(compilerState);
    JITEngine engine(compilerState);

    Node letNode;
    letNode.set_op(opLet);
    auto *ln = letNode.mutable_subnode()->mutable_let();
    auto *bn = ln->add_bindings();
    bn->set_op(opBinding);
    auto *b = bn->mutable_subnode()->mutable_binding();
    b->set_name("x");
    auto *init = b->mutable_init();
    init->set_op(opNew);
    auto *nn = init->mutable_subnode()->mutable_new_();
    nn->mutable_class_()->set_op(opConst);
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_val(
        "MyClass");
    nn->mutable_class_()->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    init->set_tag("MyClass");

    auto *body = ln->mutable_body();
    body->set_op(opDo);
    auto *dn = body->mutable_subnode()->mutable_do_();

    auto *stmt = dn->add_statements();
    stmt->set_op(opIsInstance);
    auto *is = stmt->mutable_subnode()->mutable_isinstance();
    is->set_class_("MyClass");
    auto *target = is->mutable_target();
    target->set_op(opLocal);
    target->mutable_subnode()->mutable_local()->set_name("x");
    target->mutable_subnode()->mutable_local()->set_local(localTypeLet);

    auto *ret = dn->mutable_ret();
    ret->set_op(opConst);
    ret->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNil);

    auto res = engine
                   .compileAST(letNode, "test_is_instance_refcounted_local",
                               llvm::OptimizationLevel::O0, false)
                   .get()
                   .address;
    RTValue result = resPtrToValue(res);
    assert_true(RT_isNil(result));
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_is_instance_compile_time_true),
      cmocka_unit_test(test_is_instance_compile_time_false),
      cmocka_unit_test(test_is_instance_runtime),
      cmocka_unit_test(test_is_instance_class_prefix),
      cmocka_unit_test(test_is_instance_any_type_regression),
      cmocka_unit_test(test_is_instance_refcounted_local),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
