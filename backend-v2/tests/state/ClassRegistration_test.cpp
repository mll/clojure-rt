#include "../../RuntimeHeaders.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include "../../runtime/Keyword.h"
#include "../../runtime/PersistentArrayMap.h"
#include "../../runtime/PersistentVector.h"
#include "../../runtime/RuntimeInterface.h"
#include "../../runtime/String.h"
#include "../../runtime/Symbol.h"
#include "../../runtime/tests/TestTools.h"
#include <cmocka.h>
}

#include "../../bridge/Exceptions.h"

using namespace std;
using namespace rt;

static void test_auto_registration_ids(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    rt::ThreadsafeCompilerState compState;

    // Define two classes without :object-type
    // {com.foo/A {}, com.foo/B {:extends com.foo/A}}
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    
    PersistentArrayMap *classAMap = PersistentArrayMap_empty();
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/A")),
        RT_boxPtr(classAMap));

    PersistentArrayMap *classBMap = PersistentArrayMap_empty();
    classBMap = PersistentArrayMap_assoc(
        classBMap, Keyword_create(String_create("extends")),
        Symbol_create(String_create("com.foo/A")));
    rootMap = PersistentArrayMap_assoc(
        rootMap, Symbol_create(String_create("com.foo/B")),
        RT_boxPtr(classBMap));

    compState.storeInternalClasses(RT_boxPtr(rootMap));

    ::Class *clsA = compState.classRegistry.getCurrent("com.foo/A");
    ::Class *clsB = compState.classRegistry.getCurrent("com.foo/B");

    assert_non_null(clsA);
    assert_non_null(clsB);

    // Verify IDs are >= 1000 and unique
    assert_true(clsA->registerId >= 1000);
    assert_true(clsB->registerId >= 1000);
    assert_true(clsA->registerId != clsB->registerId);

    // Verify inheritance
    assert_true(Class_isInstance(clsA, clsB));
    assert_false(Class_isInstance(clsB, clsA));

    Ptr_release(clsA);
    Ptr_release(clsB);
  });
}

int main(void) {
  initialise_memory();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_auto_registration_ids),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return result;
}
