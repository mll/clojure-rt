#include "Keyword.h"
#include "ConcurrentHashMap.h"
#include "Exceptions.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <pthread.h>

extern ConcurrentHashMap *keywords;
extern ConcurrentHashMap *keywordsInverted;

static _Atomic(uint32_t) minUnusedKeyword = 1;

#ifdef REFCOUNT_TRACING
static _Atomic(uint32_t) successfulKeywordsCount = 0;
#endif

/* mem done - consumes string */
RTValue Keyword_create(String *string) {
  RTValue stringVal = RT_boxPtr(string);
  Ptr_retain(string); /* +1 for first get */
  RTValue retVal =
      ConcurrentHashMap_get_preservesSelf(keywords, stringVal); /* consumes 1 */

  if (RT_isNil(retVal)) {
    /* If not found, we generate a new ID. In case of a race, we might waste
       an ID, which is an acceptable trade-off for a lock-free implementation.
     */
    RTValue new = RT_boxKeyword(
        atomic_fetch_add_explicit(&minUnusedKeyword, 1, memory_order_relaxed));

    /* We need to update both maps. We update keywordsInverted first so that
       any reader who finds the keyword in 'keywords' can safely look up its
       string in 'keywordsInverted'. */
    Ptr_retain(string);
    ConcurrentHashMap_assoc_preservesSelf(keywordsInverted, new, stringVal,
                                          false);

    Ptr_retain(string);
    RTValue existing = ConcurrentHashMap_putIfAbsent_preservesSelf(
        keywords, stringVal, new, false);

    if (!RT_isNil(existing)) {
      /* We lost the race. Another thread inserted the same string first.
         Return the existing keyword. */
      ConcurrentHashMap_dissoc_preservesSelf(keywordsInverted, new);
      Ptr_release(string); /* consume caller's ref */
      return existing;
    }

#ifdef REFCOUNT_TRACING
    atomic_fetch_add_explicit(&successfulKeywordsCount, 1, memory_order_relaxed);
#endif

    Ptr_release(string); /* consume caller's ref */
    return new;
  }
  Ptr_release(string); /* consume caller's ref */
  return retVal;
}

/* mem done */
String *Keyword_toString(RTValue self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get_preservesSelf(keywordsInverted, self);
  assert(!RT_isNil(retVal) &&
         "Internal error: Keyword was not interned before printing.");
  return String_concat(colon, toString(retVal));
}

uint32_t Keyword_getInternCount() {
#ifdef REFCOUNT_TRACING
  return atomic_load_explicit(&successfulKeywordsCount, memory_order_relaxed);
#else
  return atomic_load_explicit(&minUnusedKeyword, memory_order_relaxed) - 1;
#endif
}

void Keyword_resetInterns() {
  atomic_store_explicit(&minUnusedKeyword, 1, memory_order_relaxed);
#ifdef REFCOUNT_TRACING
  atomic_store_explicit(&successfulKeywordsCount, 0, memory_order_relaxed);
#endif
}

/* mem done. The assumption is that we are *sure* self is a keyword */
RTValue Keyword_invoke(RTValue self, RTValue map, RTValue defaultVal) {
  if (getType(map) == persistentArrayMapType) {
    RTValue res = PersistentArrayMap_get(RT_unboxPtr(map), self);
    if (RT_isNil(res)) {
      return defaultVal;
    }
    release(defaultVal);
    return res;
  }
  release(map);
  return defaultVal;
}
