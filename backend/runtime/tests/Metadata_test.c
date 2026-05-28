#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "../PersistentArrayMap.h"
#include "../PersistentList.h"
#include "../PersistentVector.h"
#include "../Symbol.h"
#include "../Var.h"
#include "TestTools.h"
#include "runtime/BigInteger.h"
#include "runtime/Ebr.h"
#include "runtime/RTValue.h"
#include "runtime/RuntimeInterface.h"

static void testSymbolMetadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue name = RT_boxPtr(String_create("foo"));
    Symbol *sym = Symbol_create((String *)RT_unboxPtr(name));

    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    Ptr_retain(sym);
    retain(meta);
    Symbol *symWithMeta = Symbol_withMeta(sym, meta); // consumes sym AND meta

    assert_ptr_not_equal(sym, symWithMeta);

    retain(RT_boxPtr(symWithMeta));
    RTValue retrievedMeta =
        RT_meta(RT_boxPtr(symWithMeta)); // CONSUMES symWithMeta
    assert_true(equals(meta, retrievedMeta));

    release(retrievedMeta);
    release(meta);
    Ptr_release(sym);
    Ptr_release(symWithMeta);
  });
}

static void testVarMetadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Symbol *name = Symbol_create(String_create("my-var"));
    RTValue v = RT_boxPtr(Var_create(name));

    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    retain(v);
    retain(meta);
    RTValue vWithMeta = RT_withMeta(v, meta); // consumes v AND meta

    assert_ptr_equal(RT_unboxPtr(v), RT_unboxPtr(vWithMeta));

    RTValue retrievedMeta = RT_meta(vWithMeta); // CONSUMES vWithMeta
    assert_true(equals(meta, retrievedMeta));

    release(retrievedMeta);
    release(meta);
    release(v);
    Ebr_force_reclaim();
  });
}

static void testListMetadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue l = RT_boxPtr(
        PersistentList_create(RT_boxPtr(BigInteger_createFromInt(100)), NULL));
    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    retain(l);

    RTValue lWithMeta = RT_withMeta(l, meta); // consumes l AND meta

    assert_ptr_not_equal(RT_unboxPtr(l), RT_unboxPtr(lWithMeta));
    release(l);
    RTValue retrievedMeta = RT_meta(lWithMeta); // CONSUMES lWithMeta
    assert_true(equals(meta, retrievedMeta));

    release(retrievedMeta);
    Ebr_force_reclaim();
  });
}

static void testVectorMetadata(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    RTValue vWithMeta = RT_withMeta(RT_boxPtr(v), meta); // consumes v AND meta

    assert_ptr_not_equal(v, RT_unboxPtr(vWithMeta));

    RTValue retrievedMeta = RT_meta(vWithMeta); // CONSUMES vWithMeta
    assert_true(equals(meta, retrievedMeta));

    release(retrievedMeta);
    Ebr_force_reclaim();
  });
}

static void testListMetaPropagation(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    RTValue l = RT_boxPtr(PersistentList_create(RT_boxInt32(1), NULL));
    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    RTValue lWithMeta = RT_withMeta(l, meta); // consumes l AND meta

    // PersistentList_conj consumes lWithMeta
    RTValue l2 = RT_boxPtr(PersistentList_conj(
        (PersistentList *)RT_unboxPtr(lWithMeta), RT_boxInt32(2)));

    RTValue retrievedMeta = RT_meta(l2); // CONSUMES l2
    assert_true(RT_isNil(retrievedMeta));

    release(retrievedMeta);
    Ebr_force_reclaim();
  });
}

static void testVectorMetaPropagation(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    PersistentVector *v = PersistentVector_create();
    RTValue meta = RT_boxPtr(PersistentArrayMap_create());
    RTValue vWithMeta = RT_withMeta(RT_boxPtr(v), meta); // consumes v AND meta

    // PersistentVector_conj consumes vWithMeta
    RTValue v2 = RT_boxPtr(PersistentVector_conj(
        (PersistentVector *)RT_unboxPtr(vWithMeta), RT_boxInt32(1)));

    RTValue retrievedMeta = RT_meta(v2); // CONSUMES v2
    assert_true(equals(meta, retrievedMeta));

    release(retrievedMeta);
    Ebr_force_reclaim();
  });
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(testSymbolMetadata),
      cmocka_unit_test(testVarMetadata),
      cmocka_unit_test(testListMetadata),
      cmocka_unit_test(testVectorMetadata),
      cmocka_unit_test(testListMetaPropagation),
      cmocka_unit_test(testVectorMetaPropagation),
  };
  initialise_memory();
  RuntimeInterface_initialise();
  int result = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();

  return result;
}
