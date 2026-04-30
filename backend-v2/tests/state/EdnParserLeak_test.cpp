#include "../../RuntimeHeaders.h"
#include "../../tools/EdnParser.h"
#include <cmocka.h>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include "../../runtime/Keyword.h"
#include "../../runtime/PersistentArrayMap.h"
#include "../../runtime/PersistentVector.h"
#include "../../runtime/String.h"
#include "../../runtime/Symbol.h"
#include "../../runtime/Function.h"
#include "../../runtime/tests/TestTools.h"
}

using namespace rt;

static void test_intrinsic_description_clojure_fn_leak(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    TemporaryClassData classData(RT_boxPtr(PersistentArrayMap_empty()));

    // Create a mock function
    ClojureFunction *fn = Function_create(1, 1, false);
    Function_fillMethod(fn, 0, 0, 1, false, nullptr, nullptr, 0);
    RTValue funVal = RT_boxPtr(fn);

    // Create an EDN map: {:type :clojure-function :symbol <funVal>}
    PersistentArrayMap *map = PersistentArrayMap_empty();
    map = PersistentArrayMap_assoc(
        map, Keyword_create(String_create("type")),
        Keyword_create(String_create("clojure-function")));
    map = PersistentArrayMap_assoc(map, Keyword_create(String_create("symbol")),
                                   funVal);

    {
      // IntrinsicDescription constructor consumes its first argument.
      IntrinsicDescription id(RT_boxPtr(map), classData);
      assert_int_equal((int)id.type, (int)CallType::ClojureFn);
      assert_int_equal(id.functionObject, funVal);

      // id should have retained it.
      assert_true(Object_getRawRefCount((Object *)fn) >= 2);
    }
  });
}

int main(void) {
  initialise_memory();
  RuntimeInterface_initialise();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_intrinsic_description_clojure_fn_leak),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
