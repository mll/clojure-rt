#include "Symbol.h"
#include "ConcurrentHashMap.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <pthread.h>

extern ConcurrentHashMap *symbols;
extern ConcurrentHashMap *symbolsInverted;

static pthread_mutex_t intern_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic(uint32_t) minUnusedSymbol = 1;

/* mem done - consumes string */
RTValue Symbol_create(String *string) {
  RTValue stringVal = RT_boxPtr(string);
  Ptr_retain(string); /* +1 for first get */
  RTValue retVal =
      ConcurrentHashMap_get_preservesSelf(symbols, stringVal); /* consumes 1 */

  if (RT_isNil(retVal)) {
    Ptr_retain(string); /* +1 for second get */
    pthread_mutex_lock(&intern_mutex);
    RTValue retVal2 = ConcurrentHashMap_get_preservesSelf(
        symbols, stringVal); /* consumes 1 */
    if (RT_isSymbol(retVal2)) {
      pthread_mutex_unlock(&intern_mutex);
      Ptr_release(string); /* consume caller's ref */
      return retVal2;
    }

    RTValue new = RT_boxSymbol(
        atomic_fetch_add_explicit(&minUnusedSymbol, 1, memory_order_relaxed));

    /* Need 2 refs for 2 assoc calls (each consumes stringVal as key/value).
       We currently hold 1 (caller's), so retain once more. */
    Ptr_retain(string);
    ConcurrentHashMap_assoc_preservesSelf(symbolsInverted, new, stringVal,
                                          false); /* consumes 1 */
    ConcurrentHashMap_assoc_preservesSelf(symbols, stringVal, new,
                                          false); /* consumes 1 */
    pthread_mutex_unlock(&intern_mutex);
    return new;
  }
  Ptr_release(string); /* consume caller's ref */
  return retVal;
}

/* mem done */
String *Symbol_toString(RTValue self) {
  RTValue retVal = ConcurrentHashMap_get_preservesSelf(symbolsInverted, self);
  assert(!RT_isNil(retVal) &&
         "Internal error: Symbol was not interned before printing.");
  return toString(retVal);
}

uint32_t Symbol_getInternCount() {
  return atomic_load_explicit(&minUnusedSymbol, memory_order_relaxed) - 1;
}
