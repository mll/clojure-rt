#include "TestTools.h"
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include "../Namespace.h"
#include "../Symbol.h"
#include "../Keyword.h"
#include "../Var.h"
#include "../PersistentArrayMap.h"
#include "../RuntimeInterface.h"
#include "../ConcurrentHashMap.h"

extern ConcurrentHashMap *namespaces;

#define ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS(block) \
  do { \
    MemoryState before, after; \
    captureMemoryState(&before); \
    block; \
    captureMemoryState(&after); \
    assertMemoryBalanceExcept(&before, &after, (int[]){stringType, persistentVectorType, persistentVectorNodeType}, 3); \
  } while (0)

static void test_namespace_create(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Symbol *name = Symbol_create(String_create("my.ns"));
    Namespace *ns = Namespace_create(name);
    
    assert_non_null(ns);
    assert_ptr_equal(ns->name, name);
    
    RTValue mappingsVal = atomic_load(&ns->mappings);
    assert_true(getType(mappingsVal) == persistentArrayMapType);
    
    RTValue aliasesVal = atomic_load(&ns->aliases);
    assert_true(getType(aliasesVal) == persistentArrayMapType);
    
    Ptr_release(ns);
    Ebr_force_reclaim();
  });
}

static void test_namespace_intern(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("core"));
    Namespace *ns = Namespace_create(nsSym);
    
    Symbol *varSym = Symbol_create(String_create("map"));
    
    Ptr_retain(ns); Ptr_retain(varSym);
    Var *v = Namespace_intern(ns, varSym);
    
    assert_non_null(v);
    assert_ptr_equal(v->ns, ns);
    assert_ptr_equal(v->sym, varSym);
    
    Ptr_retain(ns); Ptr_retain(varSym);
    Var *found = Namespace_findInternedVar(ns, varSym);
    assert_ptr_equal(found, v);
    if(found) Ptr_release(found);
    
    Ptr_retain(ns); Ptr_retain(varSym);
    Namespace_unmap(ns, varSym);
    
    Ptr_release(v);
    Ptr_release(ns);
    Ptr_release(varSym);
    Ebr_force_reclaim();
  });
}

static void test_namespace_aliases(void **state) {
  (void)state;
  ASSERT_MEMORY_ALL_BALANCED({
    Symbol *nsSym1 = Symbol_create(String_create("core.ns"));
    Namespace *coreNs = Namespace_create(nsSym1);
    
    Symbol *nsSym2 = Symbol_create(String_create("user.ns"));
    Namespace *userNs = Namespace_create(nsSym2);
    
    Symbol *aliasSym = Symbol_create(String_create("core"));
    
    Ptr_retain(userNs); Ptr_retain(aliasSym); Ptr_retain(coreNs);
    Namespace_addAlias(userNs, aliasSym, coreNs);
    
    Ptr_retain(userNs); Ptr_retain(aliasSym);
    Namespace *foundNs = Namespace_lookupAlias(userNs, aliasSym);
    assert_ptr_equal(foundNs, coreNs);
    if(foundNs) Ptr_release(foundNs);
    
    Ptr_retain(userNs); Ptr_retain(aliasSym);
    Namespace_removeAlias(userNs, aliasSym);
    
    Ptr_retain(userNs); Ptr_retain(aliasSym);
    Namespace *foundNsAfter = Namespace_lookupAlias(userNs, aliasSym);
    assert_null(foundNsAfter);
    
    Ptr_release(coreNs);
    Ptr_release(userNs);
    Ptr_release(aliasSym);
    Ebr_force_reclaim();
  });
}

static void test_namespace_reference(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("my.ns"));
    Namespace *ns = Namespace_create(nsSym);
    
    Symbol *sym = Symbol_create(String_create("inc"));
    Var *v = Var_create_interned(ns, sym);
    
    Ptr_retain(ns); Ptr_retain(sym); Ptr_retain(v);
    RTValue ref_val = Namespace_reference(ns, sym, RT_boxPtr((Object*)v));
    release(ref_val);
    
    Ptr_retain(ns); Ptr_retain(sym);
    RTValue mapping = Namespace_getMapping(ns, sym);
    assert_true(getType(mapping) == varType);
    assert_ptr_equal(RT_unboxPtr(mapping), v);
    if(!RT_isNil(mapping)) release(mapping);
    
    Ptr_retain(ns); Ptr_retain(sym);
    Namespace_unmap(ns, sym);
    
    Ptr_retain(ns); Ptr_retain(sym);
    RTValue mappingAfter = Namespace_getMapping(ns, sym);
    assert_true(RT_isNil(mappingAfter));
    
    Ptr_release(v);
    Ptr_release(ns);
    Ptr_release(sym);
    Ebr_force_reclaim();
  });
}

static void test_namespace_global_registry(void **state) {
  (void)state;
  MemoryState before, after;
  captureMemoryState(&before);
  {
    // Create name symbol
    Symbol *nsSym1 = Symbol_create(String_create("my.global.ns"));
    
    // Check that Namespace_find returns NULL before we register it
    Ptr_retain(nsSym1);
    Namespace *found1 = Namespace_find(nsSym1);
    assert_null(found1);
    
    // Now call findOrCreate (should create and register a new one)
    Ptr_retain(nsSym1);
    Namespace *ns1 = Namespace_findOrCreate(nsSym1);
    assert_non_null(ns1);
    assert_string_equal(String_c_str(ns1->name->name), "my.global.ns");
    
    // Call findOrCreate again (should find the existing registered one)
    Ptr_retain(nsSym1);
    Namespace *ns2 = Namespace_findOrCreate(nsSym1);
    assert_ptr_equal(ns1, ns2);
    
    // Release ns2 reference
    Ptr_release(ns2);
    
    // Call find (should find it)
    Ptr_retain(nsSym1);
    Namespace *ns3 = Namespace_find(nsSym1);
    assert_ptr_equal(ns1, ns3);
    Ptr_release(ns3);
    
    // Now let's remove it from global map so we don't have global reference leak
    Ptr_retain(nsSym1);
    RTValue symVal = RT_boxSymbol((Object *)nsSym1);
    ConcurrentHashMap_dissoc_preservesSelf(namespaces, symVal);
    
    // Release our caller's reference to ns1
    Ptr_release(ns1);
    
    // Clean up symbol reference
    Ptr_release(nsSym1);
    Ebr_force_reclaim();
  }
  captureMemoryState(&after);
  assertMemoryBalanceExcept(&before, &after, (int[]){stringType, persistentVectorType, persistentVectorNodeType, symbolType}, 4);
}

int main(void) {
  RuntimeInterface_initialise();
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_namespace_create),
      cmocka_unit_test(test_namespace_intern),
      cmocka_unit_test(test_namespace_aliases),
      cmocka_unit_test(test_namespace_reference),
      cmocka_unit_test(test_namespace_global_registry),
  };
  int res = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return res;
}
