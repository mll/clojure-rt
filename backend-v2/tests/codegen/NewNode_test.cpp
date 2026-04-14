#include "../../codegen/CodeGen.h"
#include "../../jit/JITEngine.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "../../types/ConstantClass.h"
#include "bytecode.pb.h"
#include "runtime/Class.h"
#include <fstream>
#include <iostream>

extern "C" {
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

// Mock implementation for constructor
extern "C" {
RTValue mock_Person_create(int32_t age) {
  // Return a dummy value, but we can check if it's called
  return RT_boxInt32(age + 10);
}
}

static RTValue resPtrToValue(llvm::orc::ExecutorAddr res) {
  return res.toPtr<RTValue (*)()>()();
}

static void
setup_mock_constructor_metadata(rt::ThreadsafeCompilerState &compState) {
  String *nameStr = String_create("Person");
  Ptr_retain(nameStr);
  Class *cls = Class_create(nameStr, nameStr, 0, nullptr);
  cls->registerId = 1000; // Custom ID for Person class

  ClassDescription *ext = new ClassDescription();
  ext->name = "Person";
  // We need to set the type in ClassDescription too.
  ext->type = ObjectTypeSet((objectType)1000);

  // Constructor: (Person [int] -> Person)
  IntrinsicDescription ctor;
  ctor.symbol = "mock_Person_create";
  ctor.type = CallType::Call;
  ctor.argTypes.push_back(ObjectTypeSet(integerType, false)); // age
  ctor.returnType = ObjectTypeSet((objectType)1000);
  ctor.returnsProvided = true;

  ext->constructors.push_back(ctor);

  cls->compilerExtension = ext;
  cls->compilerExtensionDestructor = delete_class_description;
  Ptr_retain(cls);
  compState.classRegistry.registerObject("Person", cls);
  compState.classRegistry.registerObject(cls, 1000);
}

static void test_new_person(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;
    setup_mock_constructor_metadata(compState);

    Node newNode;
    newNode.set_op(opNew);
    auto *n = newNode.mutable_subnode()->mutable_new_();

    // 1. Class: Person
    auto *clsNode = n->mutable_class_();
    clsNode->set_op(opConst);
    clsNode->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeClass);
    clsNode->mutable_subnode()->mutable_const_()->set_val("Person");

    // 2. Arg: age (int)
    auto *arg = n->add_args();
    arg->set_op(opConst);
    arg->mutable_subnode()->mutable_const_()->set_type(
        ConstNode_ConstType_constTypeNumber);
    arg->mutable_subnode()->mutable_const_()->set_val("25");
    arg->set_tag("long");

    try {
      auto resCall = engine
                         .compileAST(newNode, "__test_new_person")
                         .get()
                         .address;
      RTValue result = resPtrToValue(resCall);
      assert_true(RT_isInt32(result));
      assert_int_equal(35, RT_unboxInt32(result));
      release(result);
    } catch (const rt::LanguageException &e) {
      fprintf(stderr, "CodeGenerationException in test: %s\n",
              rt::getExceptionString(e).c_str());
      assert_true(false);
    } catch (const std::exception &e) {
      fprintf(stderr, "Exception in test: %s\n", e.what());
      assert_true(false);
    }
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_new_person),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);

  return result;
}
