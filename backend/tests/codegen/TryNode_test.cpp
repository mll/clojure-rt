#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "bytecode.pb.h"
#include <iostream>
#include <stdlib.h>

extern "C" {
#include "../../runtime/Exception.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"
#include "../../tools/EdnParser.h"

using namespace std;
using namespace rt;
using namespace clojure::rt::protobuf::bytecode;

static void prepareExceptionClass(uint32_t type, ThreadsafeCompilerState &compState,
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

static void test_try_catch_simple(void **state) {
  (void)state;
  rt::JITEngine engine(llvm::OptimizationLevel::O0, true);

  Node tryNode;
  tryNode.set_op(opTry);
  auto *tr = tryNode.mutable_subnode()->mutable_try_();

  auto *body = tr->mutable_body();
  body->set_op(opThrow);
  auto *th = body->mutable_subnode()->mutable_throw_();
  auto *ex = th->mutable_exception();
  RTValue msg = RT_boxPtr(String_createDynamicStr("error"));
  LanguageException *lePtr = new LanguageException("CodeGenerationException", msg, RT_boxNil());
  Exception *exObjForThrow = (Exception *)malloc(sizeof(Exception));
  Object_create((Object *)exObjForThrow, exceptionType);
  exObjForThrow->bridgedData = lePtr;
  RTValue boxedEx = RT_boxPtr(exObjForThrow);

  rt::ThreadsafeCompilerState& threadsafeState = engine.threadsafeState;
  prepareExceptionClass(exceptionType, threadsafeState, "CodeGenerationException");

  Var *var = threadsafeState.getOrCreateVar("my_exception_var");
  var->root = boxedEx;
  var->dynamic = false;
  
  ex->set_op(opVar);
  ex->mutable_subnode()->mutable_var()->set_var("#'my_exception_var");

  auto *catchClauseNode = tr->add_catches();
  catchClauseNode->set_op(opCatch);
  auto *catchClause = catchClauseNode->mutable_subnode()->mutable_catch_();

  auto *classConstNode = catchClause->mutable_class_();
  classConstNode->set_op(opConst);
  classConstNode->mutable_subnode()->mutable_const_()->set_type(
      ConstNode_ConstType_constTypeString);
  classConstNode->mutable_subnode()->mutable_const_()->set_val(
      "CodeGenerationException");

  auto *localBinding = catchClause->mutable_local();
  localBinding->set_op(opBinding);
  localBinding->mutable_subnode()->mutable_binding()->set_name("e");

  auto *catchBody = catchClause->mutable_body();
  catchBody->set_op(opLocal);
  catchBody->mutable_subnode()->mutable_local()->set_name("e");

  // We expect this to throw an "CodeGenerationException" (because string is thrown),
  // be caught by our catch clause, and return the exception object itself.
  auto resThrow = engine.compileAST(tryNode, "__test_try_catch_simple").get();
  std::cout << "IR DUMP:" << std::endl << resThrow.optimizedIR << std::endl;
  RTValue result = resThrow.address.toPtr<RTValue (*)()>()();
  
  // Result should be the caught exception.
  assert_true(RT_isPtr(result));
  
  Exception *exObj = (Exception *)RT_unboxPtr(result);
  assert_true(exObj->header.type == exceptionType);

  LanguageException *le = static_cast<LanguageException*>(exObj->bridgedData);
  assert_string_equal("CodeGenerationException", le->getName().c_str());
  
  Object_release((Object*)exObj);
}

static void test_try_catch_nested_throw(void **state) {
  (void)state;
  rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
  rt::ThreadsafeCompilerState& threadsafeState = engine.threadsafeState;
  prepareExceptionClass(exceptionType, threadsafeState, "CodeGenerationException");

  // Create Exception A
  RTValue msgA = RT_boxPtr(String_createDynamicStr("error A"));
  LanguageException *lePtrA = new LanguageException("CodeGenerationException", msgA, RT_boxNil());
  Exception *exObjForThrowA = (Exception *)malloc(sizeof(Exception));
  Object_create((Object *)exObjForThrowA, exceptionType);
  exObjForThrowA->bridgedData = lePtrA;
  Var *varA = threadsafeState.getOrCreateVar("my_exception_var_A");
  varA->root = RT_boxPtr(exObjForThrowA);
  varA->dynamic = false;

  // Create Exception B
  RTValue msgB = RT_boxPtr(String_createDynamicStr("error B"));
  LanguageException *lePtrB = new LanguageException("CodeGenerationException", msgB, RT_boxNil());
  Exception *exObjForThrowB = (Exception *)malloc(sizeof(Exception));
  Object_create((Object *)exObjForThrowB, exceptionType);
  exObjForThrowB->bridgedData = lePtrB;
  Var *varB = threadsafeState.getOrCreateVar("my_exception_var_B");
  varB->root = RT_boxPtr(exObjForThrowB);
  varB->dynamic = false;

  Node tryNode;
  tryNode.set_op(opTry);
  auto *tr = tryNode.mutable_subnode()->mutable_try_();

  auto *body = tr->mutable_body();
  body->set_op(opThrow);
  auto *th = body->mutable_subnode()->mutable_throw_();
  th->mutable_exception()->set_op(opVar);
  th->mutable_exception()->mutable_subnode()->mutable_var()->set_var("#'my_exception_var_A");

  auto *catchClauseNode = tr->add_catches();
  catchClauseNode->set_op(opCatch);
  auto *catchClause = catchClauseNode->mutable_subnode()->mutable_catch_();

  auto *classConstNode = catchClause->mutable_class_();
  classConstNode->set_op(opConst);
  classConstNode->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeString);
  classConstNode->mutable_subnode()->mutable_const_()->set_val("CodeGenerationException");

  auto *localBinding = catchClause->mutable_local();
  localBinding->set_op(opBinding);
  localBinding->mutable_subnode()->mutable_binding()->set_name("e");

  // Catch body throws Exception B
  auto *catchBody = catchClause->mutable_body();
  catchBody->set_op(opThrow);
  auto *catchTh = catchBody->mutable_subnode()->mutable_throw_();
  catchTh->mutable_exception()->set_op(opVar);
  catchTh->mutable_exception()->mutable_subnode()->mutable_var()->set_var("#'my_exception_var_B");

  auto resThrow = engine.compileAST(tryNode, "__test_try_catch_nested_throw").get();
  
  bool caught = false;
  try {
    resThrow.address.toPtr<RTValue (*)()>()();
  } catch (const LanguageException& e) {
    caught = true;
    String* msgStr = (String*)RT_unboxPtr(e.getMessage());
    assert_string_equal("error B", String_c_str(msgStr));
  }
  assert_true(caught);
}

static void test_try_finally_unwind(void **state) {
  (void)state;
  rt::JITEngine engine(llvm::OptimizationLevel::O0, true);
  rt::ThreadsafeCompilerState& threadsafeState = engine.threadsafeState;
  prepareExceptionClass(exceptionType, threadsafeState, "CodeGenerationException");

  RTValue msgA = RT_boxPtr(String_createDynamicStr("error unwind"));
  LanguageException *lePtrA = new LanguageException("CodeGenerationException", msgA, RT_boxNil());
  Exception *exObjForThrowA = (Exception *)malloc(sizeof(Exception));
  Object_create((Object *)exObjForThrowA, exceptionType);
  exObjForThrowA->bridgedData = lePtrA;
  Var *varA = threadsafeState.getOrCreateVar("my_exception_var_unwind");
  varA->root = RT_boxPtr(exObjForThrowA);
  varA->dynamic = false;

  Node tryNode;
  tryNode.set_op(opTry);
  auto *tr = tryNode.mutable_subnode()->mutable_try_();

  auto *body = tr->mutable_body();
  body->set_op(opThrow);
  auto *th = body->mutable_subnode()->mutable_throw_();
  th->mutable_exception()->set_op(opVar);
  th->mutable_exception()->mutable_subnode()->mutable_var()->set_var("#'my_exception_var_unwind");

  // finally body just returns a constant 42 (which is ignored because unwind continues)
  auto *finallyNode = tr->mutable_finally();
  finallyNode->set_op(opConst);
  finallyNode->mutable_subnode()->mutable_const_()->set_type(ConstNode_ConstType_constTypeNumber);
  finallyNode->mutable_subnode()->mutable_const_()->set_val("42");

  auto resThrow = engine.compileAST(tryNode, "__test_try_finally_unwind").get();
  
  bool caught = false;
  try {
    resThrow.address.toPtr<RTValue (*)()>()();
  } catch (const LanguageException& e) {
    caught = true;
    String* msgStr = (String*)RT_unboxPtr(e.getMessage());
    assert_string_equal("error unwind", String_c_str(msgStr));
  }
  assert_true(caught);
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_try_catch_simple),
      cmocka_unit_test(test_try_catch_nested_throw),
      cmocka_unit_test(test_try_finally_unwind),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);

  return result;
}
