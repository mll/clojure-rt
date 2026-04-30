#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

extern "C" {
#include "../../runtime/Keyword.h"
#include "../../runtime/PersistentArrayMap.h"
#include "../../runtime/String.h"
#include "../../runtime/Symbol.h"
#include "../../runtime/Function.h"
#include "../../runtime/tests/TestTools.h"
}

using namespace rt;

static void test_extend_internal_classes_merge(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // 1. Initial store
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    PersistentArrayMap *classAMap = PersistentArrayMap_empty();
    PersistentArrayMap *fieldsMap = PersistentArrayMap_empty();
    fieldsMap = PersistentArrayMap_assoc(fieldsMap, Symbol_create(String_create("f1")), RT_boxPtr(BigInteger_createFromInt(10)));
    classAMap = PersistentArrayMap_assoc(classAMap, Keyword_create(String_create("static-fields")), RT_boxPtr(fieldsMap));
    rootMap = PersistentArrayMap_assoc(rootMap, Symbol_create(String_create("com.foo/A")), RT_boxPtr(classAMap));

    compState.storeInternalClasses(RT_boxPtr(rootMap));

    // 2. Extension
    PersistentArrayMap *extMap = PersistentArrayMap_empty();
    PersistentArrayMap *classAExt = PersistentArrayMap_empty();
    PersistentArrayMap *fieldsExt = PersistentArrayMap_empty();
    fieldsExt = PersistentArrayMap_assoc(fieldsExt, Symbol_create(String_create("f2")), RT_boxPtr(BigInteger_createFromInt(20)));
    classAExt = PersistentArrayMap_assoc(classAExt, Keyword_create(String_create("static-fields")), RT_boxPtr(fieldsExt));
    extMap = PersistentArrayMap_assoc(extMap, Symbol_create(String_create("com.foo/A")), RT_boxPtr(classAExt));

    compState.extendInternalClasses(RT_boxPtr(extMap));

    // 3. Verify
    ::Class *clsA = compState.classRegistry.getCurrent("com.foo/A");
    assert_non_null(clsA);
    ClassDescription *desc = static_cast<ClassDescription *>(clsA->compilerExtension);
    
    assert_int_equal(desc->staticFields.size(), 2);
    assert_true(BigInteger_equalsInt(static_cast<BigInteger *>(RT_unboxPtr(desc->staticFields["f1"])), 10));
    assert_true(BigInteger_equalsInt(static_cast<BigInteger *>(RT_unboxPtr(desc->staticFields["f2"])), 20));

    Ptr_release(clsA);
  });
}

static void test_extend_internal_classes_function_merge(void **state) {
  (void)state;

  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // 1. Initial store
    PersistentArrayMap *rootMap = PersistentArrayMap_empty();
    PersistentArrayMap *classBMap = PersistentArrayMap_empty();
    PersistentArrayMap *fnsMap = PersistentArrayMap_empty();
    
    PersistentArrayMap *fn1Desc = PersistentArrayMap_empty();
    fn1Desc = PersistentArrayMap_assoc(fn1Desc, Keyword_create(String_create("args")), RT_boxPtr(PersistentVector_create()));
    fn1Desc = PersistentArrayMap_assoc(fn1Desc, Keyword_create(String_create("type")), Keyword_create(String_create("call")));
    fn1Desc = PersistentArrayMap_assoc(fn1Desc, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("S1")));
    
    PersistentVector *fn1Vec = PersistentVector_createMany(1, RT_boxPtr(fn1Desc));
    
    fnsMap = PersistentArrayMap_assoc(fnsMap, Symbol_create(String_create("fn1")), RT_boxPtr(fn1Vec));
    classBMap = PersistentArrayMap_assoc(classBMap, Keyword_create(String_create("static-fns")), RT_boxPtr(fnsMap));
    rootMap = PersistentArrayMap_assoc(rootMap, Symbol_create(String_create("com.foo/B")), RT_boxPtr(classBMap));

    compState.storeInternalClasses(RT_boxPtr(rootMap));

    // 2. Extension
    PersistentArrayMap *extMap = PersistentArrayMap_empty();
    PersistentArrayMap *classBExt = PersistentArrayMap_empty();
    PersistentArrayMap *fnsExt = PersistentArrayMap_empty();
    
    PersistentArrayMap *fn2Desc = PersistentArrayMap_empty();
    fn2Desc = PersistentArrayMap_assoc(fn2Desc, Keyword_create(String_create("args")), RT_boxPtr(PersistentVector_create()));
    fn2Desc = PersistentArrayMap_assoc(fn2Desc, Keyword_create(String_create("type")), Keyword_create(String_create("call")));
    fn2Desc = PersistentArrayMap_assoc(fn2Desc, Keyword_create(String_create("symbol")), RT_boxPtr(String_create("S2")));
    
    PersistentVector *fn2Vec = PersistentVector_createMany(1, RT_boxPtr(fn2Desc));
    
    fnsExt = PersistentArrayMap_assoc(fnsExt, Symbol_create(String_create("fn2")), RT_boxPtr(fn2Vec));
    classBExt = PersistentArrayMap_assoc(classBExt, Keyword_create(String_create("static-fns")), RT_boxPtr(fnsExt));
    extMap = PersistentArrayMap_assoc(extMap, Symbol_create(String_create("com.foo/B")), RT_boxPtr(classBExt));

    compState.extendInternalClasses(RT_boxPtr(extMap));

    // 3. Verify
    ::Class *clsB = compState.classRegistry.getCurrent("com.foo/B");
    assert_non_null(clsB);
    ClassDescription *desc = static_cast<ClassDescription *>(clsB->compilerExtension);
    
    assert_int_equal(desc->staticFns.size(), 2);
    assert_int_equal(desc->staticFns["fn1"].size(), 1);
    assert_int_equal(desc->staticFns["fn2"].size(), 1);
    assert_string_equal(desc->staticFns["fn1"][0].symbol.c_str(), "S1");
    assert_string_equal(desc->staticFns["fn2"][0].symbol.c_str(), "S2");

    Ptr_release(clsB);
  });
}

int main(void) {
  initialise_memory();

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_extend_internal_classes_merge),
      cmocka_unit_test(test_extend_internal_classes_function_merge),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
