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
#include "../ExecutionContext.h"

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
    RTValue foundVal = Namespace_findInternedVar(ns, varSym);
    assert_false(RT_isNil(foundVal));
    Var *found = (Var *)RT_unboxPtr(foundVal);
    assert_ptr_equal(found, v);
    release(foundVal);
    
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
    RTValue foundNsVal = Namespace_lookupAlias(userNs, aliasSym);
    assert_false(RT_isNil(foundNsVal));
    Namespace *foundNs = (Namespace *)RT_unboxPtr(foundNsVal);
    assert_ptr_equal(foundNs, coreNs);
    release(foundNsVal);
    
    Ptr_retain(userNs); Ptr_retain(aliasSym);
    Namespace_removeAlias(userNs, aliasSym);
    
    Ptr_retain(userNs); Ptr_retain(aliasSym);
    RTValue foundNsAfterVal = Namespace_lookupAlias(userNs, aliasSym);
    assert_true(RT_isNil(foundNsAfterVal));
    release(foundNsAfterVal);
    
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
    Ptr_retain(ns);
    Ptr_retain(sym);
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
    Ptr_release(sym);
    Ptr_release(ns);
    
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
    
    // Check that Namespace_find returns nil before we register it
    Ptr_retain(nsSym1);
    RTValue found1Val = Namespace_find(nsSym1);
    assert_true(RT_isNil(found1Val));
    release(found1Val);
    
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
    RTValue ns3Val = Namespace_find(nsSym1);
    assert_false(RT_isNil(ns3Val));
    Namespace *ns3 = (Namespace *)RT_unboxPtr(ns3Val);
    assert_ptr_equal(ns1, ns3);
    release(ns3Val);
    
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

static void test_namespace_var_circular_leak(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("temp-ns"));
    Namespace *ns = Namespace_create(nsSym);

    Symbol *varSym = Symbol_create(String_create("temp-var"));
    
    Ptr_retain(ns); Ptr_retain(varSym);
    Var *v = Namespace_intern(ns, varSym);
    
    Ptr_release(v);
    Ptr_release(ns);
    Ptr_release(varSym);
    Ebr_force_reclaim();
  });
}

static void test_namespace_redefine_referred_var_should_throw(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("my.ns"));
    Namespace *ns = Namespace_create(nsSym);
    
    Symbol *otherNsSym = Symbol_create(String_create("other.ns"));
    Namespace *otherNs = Namespace_create(otherNsSym);
    
    Symbol *sym = Symbol_create(String_create("foo"));
    
    Ptr_retain(otherNs); Ptr_retain(sym);
    Var *otherVar = Var_create_interned(otherNs, sym);
    
    // Refer otherVar in ns under "foo"
    Ptr_retain(ns); Ptr_retain(sym); Ptr_retain(otherVar);
    Var *ref = Namespace_refer(ns, sym, otherVar);
    Ptr_release(ref);
    
    // Now try to intern "foo" in ns. Replacement is allowed because the old value was a referred Var (non-interned),
    // so it should succeed and return the newly created interned Var.
    Ptr_retain(ns); Ptr_retain(sym);
    Var *v = Namespace_intern(ns, sym);
    assert_non_null(v);
    assert_ptr_equal(v->ns, ns);
    assert_ptr_equal(v->sym, sym);
    Ptr_release(v);
    
    Ptr_release(otherVar);
    Ptr_release(otherNs);
    Ptr_release(ns);
    Ptr_release(sym);
    Ebr_force_reclaim();
  });
}

static void test_namespace_redefine_class_should_throw(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("my.ns"));
    Namespace *ns = Namespace_create(nsSym);
    
    Symbol *sym = Symbol_create(String_create("foo"));
    
    // Map an arbitrary non-Var value, say a String, to sym
    String *strVal = String_create("some class or other value");
    
    Ptr_retain(ns); Ptr_retain(sym); retain(RT_boxPtr((Object *)strVal));
    RTValue ref = Namespace_reference(ns, sym, RT_boxPtr((Object *)strVal));
    release(ref);
    
    // Now try to intern "foo" in ns. Replacement is allowed because the old value was a String (non-Var),
    // so it should succeed and return the newly created interned Var.
    Ptr_retain(ns); Ptr_retain(sym);
    Var *v = Namespace_intern(ns, sym);
    assert_non_null(v);
    assert_ptr_equal(v->ns, ns);
    assert_ptr_equal(v->sym, sym);
    Ptr_release(v);
    
    Ptr_release(ns);
    Ptr_release(sym);
    Ptr_release(strVal);
    Ebr_force_reclaim();
  });
}

static void test_namespace_redefine_referred_var_with_other_reference_should_throw(void **state) {
  (void)state;
  ASSERT_MEMORY_BALANCED_EXCEPT_STRINGS({
    Symbol *nsSym = Symbol_create(String_create("my.ns"));
    Namespace *ns = Namespace_create(nsSym);
    
    Symbol *otherNsSym = Symbol_create(String_create("other.ns"));
    Namespace *otherNs = Namespace_create(otherNsSym);
    
    Symbol *thirdNsSym = Symbol_create(String_create("third.ns"));
    Namespace *thirdNs = Namespace_create(thirdNsSym);
    
    Symbol *sym = Symbol_create(String_create("foo"));
    
    Ptr_retain(otherNs); Ptr_retain(sym);
    Var *otherVar = Var_create_interned(otherNs, sym);
    
    Ptr_retain(thirdNs); Ptr_retain(sym);
    Var *thirdVar = Var_create_interned(thirdNs, sym);
    
    // Refer otherVar in ns under "foo"
    Ptr_retain(ns); Ptr_retain(sym); Ptr_retain(otherVar);
    Var *ref1 = Namespace_refer(ns, sym, otherVar);
    Ptr_release(ref1);
    
    // Now try to refer thirdVar in ns under "foo". Replacement is allowed (with a warning),
    // so it should succeed and return thirdVar.
    Ptr_retain(ns); Ptr_retain(sym); Ptr_retain(thirdVar);
    Var *ref2 = Namespace_refer(ns, sym, thirdVar);
    assert_ptr_equal(ref2, thirdVar);
    Ptr_release(ref2);
    
    Ptr_release(otherVar);
    Ptr_release(thirdVar);
    Ptr_release(otherNs);
    Ptr_release(thirdNs);
    Ptr_release(ns);
    Ptr_release(sym);
    Ebr_force_reclaim();
  });
}

static void test_rt_in_ns(void **state) {
  (void)state;
  MemoryState before, after;
  captureMemoryState(&before);
  {
    // Setup execution context with empty bindings map
    ExecutionContext *ctx = ExecutionContext_create(RT_boxPtr(PersistentArrayMap_empty()));
    
    // Setup clojure.core namespace and *ns* Var (like initializeDefaultNamespaces does)
    Symbol *coreSym = Symbol_create(String_create("clojure.core"));
    Namespace *coreNs = Namespace_findOrCreate(coreSym);
    
    Symbol *nsSym = Symbol_create(String_create("*ns*"));
    Var *nsVar = Namespace_intern(coreNs, nsSym);
    Var_setDynamic(nsVar, true);
    
    // Set initial bindings map: map *ns* to user namespace
    Symbol *userSym = Symbol_create(String_create("user"));
    Namespace *userNs = Namespace_findOrCreate(userSym);
    Ptr_retain(nsVar);
    Var_bindRoot(nsVar, RT_boxPtr((Object *)userNs));
    
    RTValue oldBindings = ctx->bindingsMap;
    Ptr_retain(RT_unboxPtr(oldBindings));
    Ptr_retain(nsVar);
    Ptr_retain(userNs);
    ctx->bindingsMap = RT_boxPtr(PersistentArrayMap_assoc(
        (PersistentArrayMap *)RT_unboxPtr(oldBindings),
        RT_boxPtr(nsVar),
        RT_boxPtr((Object *)userNs)));
    release(oldBindings);
        
    // Now call RT_inNs to switch to "my-new-ns"
    Symbol *newNsSym = Symbol_create(String_create("my-new-ns"));
    Ptr_retain(newNsSym);
    RTValue res = RT_inNs(ctx, newNsSym);
    assert_false(RT_isNil(res));
    Namespace *newNs = (Namespace *)RT_unboxPtr(res);
    assert_string_equal(String_c_str(newNs->name->name), "my-new-ns");
    
    // Check that the bindings map has indeed been updated to "my-new-ns"
    Ptr_retain(RT_unboxPtr(ctx->bindingsMap));
    Ptr_retain(nsVar);
    RTValue currentNsVal = PersistentArrayMap_get(
        (PersistentArrayMap *)RT_unboxPtr(ctx->bindingsMap),
        RT_boxPtr(nsVar));
    assert_ptr_equal(RT_unboxPtr(currentNsVal), newNs);
    release(currentNsVal);
    
    // Clean up
    release(res);
    Ptr_release(ctx);
    Ptr_release(nsVar);
    
    // We created "clojure.core", "user" and "my-new-ns". Let's dissoc them from the global map to prevent leaks:
    ConcurrentHashMap_dissoc_preservesSelf(namespaces, RT_boxSymbol((Object *)Symbol_create(String_create("clojure.core"))));
    ConcurrentHashMap_dissoc_preservesSelf(namespaces, RT_boxSymbol((Object *)Symbol_create(String_create("user"))));
    ConcurrentHashMap_dissoc_preservesSelf(namespaces, RT_boxSymbol((Object *)Symbol_create(String_create("my-new-ns"))));
    Ptr_release(newNsSym);
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
      cmocka_unit_test(test_namespace_var_circular_leak),
      cmocka_unit_test(test_namespace_redefine_referred_var_should_throw),
      cmocka_unit_test(test_namespace_redefine_class_should_throw),
      cmocka_unit_test(test_namespace_redefine_referred_var_with_other_reference_should_throw),
      cmocka_unit_test(test_rt_in_ns),
  };
  int res = cmocka_run_group_tests(tests, NULL, NULL);
  RuntimeInterface_cleanup();
  return res;
}
