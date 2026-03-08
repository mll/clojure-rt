#include "Keyword.h"
#include "ConcurrentHashMap.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <pthread.h>

extern ConcurrentHashMap *keywords;
extern ConcurrentHashMap *keywordsInverted;

static pthread_mutex_t intern_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic uint32_t minUnusedKeyword = 1;

/* mem done */
RTValue Keyword_create(String *string) {
  RTValue stringVal = RT_boxPtr(string);
  Ptr_retain(string);
  RTValue retVal = ConcurrentHashMap_get(keywords, stringVal);

  if (RT_isNil(retVal)) {
    Ptr_retain(string);
    pthread_mutex_lock(&intern_mutex);
    /* interning */
    RTValue retVal2 = ConcurrentHashMap_get(keywords, stringVal);
    if (RT_isKeyword(retVal2)) {
      pthread_mutex_unlock(&intern_mutex);
      Ptr_release(string);
      return retVal2;
    }

    RTValue new = RT_boxKeyword(
        atomic_fetch_add_explicit(&minUnusedKeyword, 1, memory_order_relaxed));

    Ptr_retain(string);
    ConcurrentHashMap_assoc(keywordsInverted, new, stringVal);
    Ptr_retain(string);
    ConcurrentHashMap_assoc(keywords, stringVal, new);
    pthread_mutex_unlock(&intern_mutex);
    Ptr_release(string);
    return new;
  }
  Ptr_release(string);
  return retVal;
}

/* mem done */
String *Keyword_toString(RTValue self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get(keywordsInverted, self);
  assert(!RT_isNil(retVal) &&
         "Internal error: Keyword was not interned before printing.");
  return String_concat(colon, toString(retVal));
}

uint32_t Keyword_getInternCount() {
  return atomic_load_explicit(&minUnusedKeyword, memory_order_relaxed) - 1;
}
