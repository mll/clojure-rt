#include "Object.h"
#include "Keyword.h"
#include "RTValue.h"
#include "String.h"
#include "ConcurrentHashMap.h"
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
    if (RT_isKeyword(retVal)) {
      pthread_mutex_unlock(&intern_mutex);
      Ptr_release(string);
      return retVal2;
    }      
    
    RTValue new = RT_boxKeyword(
        atomic_fetch_add_explicit(&minUnusedKeyword, 1, memory_order_relaxed));

    Ptr_retain(string);
    ConcurrentHashMap_assoc(keywordsInverted, new, stringVal);
    ConcurrentHashMap_assoc(keywords, stringVal, new);
    pthread_mutex_unlock(&intern_mutex);
    return new;
  }
  Ptr_release(string);
  return retVal;
}

/* mem done */
String *Keyword_toString(RTValue self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get(keywordsInverted, self);
  assert(!RT_isNil(retVal) && "Internal error: Keyword was not interned before printing.");
  return String_concat(colon, toString(retVal));
}


