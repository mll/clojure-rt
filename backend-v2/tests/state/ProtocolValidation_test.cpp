#include "../../RuntimeHeaders.h"
#include "../../jit/JITEngine.h"
#include "../../state/ThreadsafeCompilerState.h"
#include "../../tools/EdnParser.h"
#include "bytecode.pb.h"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

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

static void test_protocol_inheritance_and_isinstance(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // 1. Define Protocols
    PersistentArrayMap *p1Map = PersistentArrayMap_empty();
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("is-interface")), RT_boxBool(true));
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("name")),
        RT_boxPtr(String_create("P1")));
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("object-type")), RT_boxInt32(2001));

    PersistentArrayMap *p1Fns = PersistentArrayMap_empty();
    PersistentVector *p1Sig = PersistentVector_create();
    p1Sig = PersistentVector_conj(p1Sig, Keyword_create(String_create("this")));

    PersistentArrayMap *f1Desc = PersistentArrayMap_empty();
    f1Desc = PersistentArrayMap_assoc(f1Desc, Keyword_create(String_create("args")),
                                    RT_boxPtr(p1Sig));
    f1Desc = PersistentArrayMap_assoc(
        f1Desc, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    f1Desc = PersistentArrayMap_assoc(
        f1Desc, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("f1_sym")));
    PersistentVector *f1Overloads = PersistentVector_create();
    f1Overloads = PersistentVector_conj(f1Overloads, RT_boxPtr(f1Desc));
    p1Fns = PersistentArrayMap_assoc(
        p1Fns, Symbol_create(String_create("f1")), RT_boxPtr(f1Overloads));
    Ptr_retain(f1Overloads); // For p2Impl later

    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("instance-fns")), RT_boxPtr(p1Fns));

    PersistentArrayMap *p2Map = PersistentArrayMap_empty();
    p2Map = PersistentArrayMap_assoc(
        p2Map, Keyword_create(String_create("is-interface")), RT_boxBool(true));
    p2Map = PersistentArrayMap_assoc(
        p2Map, Keyword_create(String_create("name")),
        RT_boxPtr(String_create("P2")));
    p2Map = PersistentArrayMap_assoc(
        p2Map, Keyword_create(String_create("object-type")), RT_boxInt32(2002));
    p2Map = PersistentArrayMap_assoc(
        p2Map, Keyword_create(String_create("extends")),
        Symbol_create(String_create("P1")));
    PersistentArrayMap *p2Fns = PersistentArrayMap_empty();
    PersistentVector *p2Sig = PersistentVector_create();
    p2Sig = PersistentVector_conj(p2Sig, Keyword_create(String_create("this")));
    p2Sig = PersistentVector_conj(p2Sig, Keyword_create(String_create("any")));

    PersistentArrayMap *f2Desc = PersistentArrayMap_empty();
    f2Desc = PersistentArrayMap_assoc(f2Desc, Keyword_create(String_create("args")),
                                    RT_boxPtr(p2Sig));
    f2Desc = PersistentArrayMap_assoc(
        f2Desc, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    f2Desc = PersistentArrayMap_assoc(
        f2Desc, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("f2_sym")));
    PersistentVector *f2Overloads = PersistentVector_create();
    f2Overloads = PersistentVector_conj(f2Overloads, RT_boxPtr(f2Desc));
    p2Fns = PersistentArrayMap_assoc(
        p2Fns, Symbol_create(String_create("f2")), RT_boxPtr(f2Overloads));
    Ptr_retain(f2Overloads); // For p2Impl later

    p2Map = PersistentArrayMap_assoc(
        p2Map, Keyword_create(String_create("instance-fns")), RT_boxPtr(p2Fns));

    PersistentArrayMap *protoRoot = PersistentArrayMap_empty();
    protoRoot = PersistentArrayMap_assoc(
        protoRoot, Symbol_create(String_create("P1")), RT_boxPtr(p1Map));
    protoRoot = PersistentArrayMap_assoc(
        protoRoot, Symbol_create(String_create("P2")), RT_boxPtr(p2Map));

    compState.storeInternalProtocols(RT_boxPtr(protoRoot));

    // 2. Define Class implementing P2
    PersistentArrayMap *c1Map = PersistentArrayMap_empty();
    c1Map = PersistentArrayMap_assoc(
        c1Map, Keyword_create(String_create("object-type")), RT_boxInt32(1001));
    c1Map = PersistentArrayMap_assoc(c1Map, Keyword_create(String_create("name")),
                                   RT_boxPtr(String_create("C1")));

    PersistentArrayMap *p2Impl = PersistentArrayMap_empty();
    // P2 requires f1 (from P1) and f2
    p2Impl = PersistentArrayMap_assoc(p2Impl, Symbol_create(String_create("f1")),
                                    RT_boxPtr(f1Overloads));
    // f1Overloads was retained above
    p2Impl = PersistentArrayMap_assoc(p2Impl, Symbol_create(String_create("f2")),
                                    RT_boxPtr(f2Overloads));
    // f2Overloads was retained above

    PersistentArrayMap *impls = PersistentArrayMap_empty();
    impls = PersistentArrayMap_assoc(
        impls, Symbol_create(String_create("P2")), RT_boxPtr(p2Impl));
    c1Map = PersistentArrayMap_assoc(
        c1Map, Keyword_create(String_create("implements")), RT_boxPtr(impls));

    PersistentArrayMap *classRoot = PersistentArrayMap_empty();
    classRoot = PersistentArrayMap_assoc(
        classRoot, Symbol_create(String_create("C1")), RT_boxPtr(c1Map));

    compState.storeInternalClasses(RT_boxPtr(classRoot));

    // 3. Verify
    ::Class *clsC1 = compState.classRegistry.getCurrent("C1");
    ::Class *clsP1 = compState.protocolRegistry.getCurrent("P1");
    ::Class *clsP2 = compState.protocolRegistry.getCurrent("P2");

    assert_non_null(clsC1);
    assert_non_null(clsP1);
    assert_non_null(clsP2);

    // Note: Class_isInstance won't work for protocols since they are external
    // but we can check the implementedProtocols directly in the class
    ClassList *list = atomic_load(&clsC1->implementedProtocols);
    assert_non_null(list);
    assert_int_equal(1, list->count);
    assert_ptr_equal(clsP2, list->classes[0]);

    Ptr_release(clsC1);
    Ptr_release(clsP1);
    Ptr_release(clsP2);
  });
}

static void test_missing_method_fails(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    rt::JITEngine engine;
    rt::ThreadsafeCompilerState &compState = engine.threadsafeState;

    // 1. Define Protocol P1 with method f1
    PersistentArrayMap *p1Map = PersistentArrayMap_empty();
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("is-interface")), RT_boxBool(true));
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("name")),
        RT_boxPtr(String_create("P1")));

    PersistentArrayMap *p1Fns = PersistentArrayMap_empty();
    PersistentVector *p1Sig = PersistentVector_create();
    p1Sig = PersistentVector_conj(p1Sig, Keyword_create(String_create("this")));

    PersistentArrayMap *f1Desc = PersistentArrayMap_empty();
    f1Desc = PersistentArrayMap_assoc(f1Desc, Keyword_create(String_create("args")),
                                    RT_boxPtr(p1Sig));
    f1Desc = PersistentArrayMap_assoc(
        f1Desc, Keyword_create(String_create("type")),
        Keyword_create(String_create("call")));
    f1Desc = PersistentArrayMap_assoc(
        f1Desc, Keyword_create(String_create("symbol")),
        RT_boxPtr(String_create("f1_sym")));
    PersistentVector *f1Overloads = PersistentVector_create();
    f1Overloads = PersistentVector_conj(f1Overloads, RT_boxPtr(f1Desc));
    p1Fns = PersistentArrayMap_assoc(
        p1Fns, Symbol_create(String_create("f1")), RT_boxPtr(f1Overloads));
    p1Map = PersistentArrayMap_assoc(
        p1Map, Keyword_create(String_create("instance-fns")), RT_boxPtr(p1Fns));

    PersistentArrayMap *protoRoot = PersistentArrayMap_empty();
    protoRoot = PersistentArrayMap_assoc(
        protoRoot, Symbol_create(String_create("P1")), RT_boxPtr(p1Map));

    compState.storeInternalProtocols(RT_boxPtr(protoRoot));

    // 2. Define Class C1 claiming to implement P1, but missing f1
    PersistentArrayMap *c1Map = PersistentArrayMap_empty();
    c1Map = PersistentArrayMap_assoc(
        c1Map, Keyword_create(String_create("object-type")), RT_boxInt32(1001));
    c1Map = PersistentArrayMap_assoc(c1Map, Keyword_create(String_create("name")),
                                   RT_boxPtr(String_create("C1")));

    PersistentArrayMap *p1Impl = PersistentArrayMap_empty();
    // Missing f1 in the implementation map
    PersistentArrayMap *impls = PersistentArrayMap_empty();
    impls = PersistentArrayMap_assoc(
        impls, Symbol_create(String_create("P1")), RT_boxPtr(p1Impl));
    c1Map = PersistentArrayMap_assoc(
        c1Map, Keyword_create(String_create("implements")), RT_boxPtr(impls));

    PersistentArrayMap *classRoot = PersistentArrayMap_empty();
    classRoot = PersistentArrayMap_assoc(
        classRoot, Symbol_create(String_create("C1")), RT_boxPtr(c1Map));

    // 3. Validation should throw during storeInternalClasses because C1 is missing f1
    bool thrown = false;
    try {
      compState.storeInternalClasses(RT_boxPtr(classRoot));
    } catch (const rt::LanguageException &e) {
      thrown = true;
    }
    assert_true(thrown);

    // Cleanup
    ::Class *clsC1 = compState.classRegistry.getCurrent("C1");
    if (clsC1) Ptr_release(clsC1);
  });
}

int main(void) {
  initialise_memory();
  

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_protocol_inheritance_and_isinstance),
      cmocka_unit_test(test_missing_method_fails),
  };

  int result = cmocka_run_group_tests(tests, NULL, NULL);
  return result;
}
