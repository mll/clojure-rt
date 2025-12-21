#include "Object.h"
#include "Keyword.h"
#include "RTValue.h"
#include "String.h"
#include "ConcurrentHashMap.h"
#include <pthread.h>

extern ConcurrentHashMap *keywords;
extern ConcurrentHashMap *keywordsInverted;

static pthread_mutex_t intern_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic uint32_t minUnusedKeyword = 0;

/* mem done */
RTValue Keyword_create(String *string) {
  retain(string);  
  RTValue retVal = ConcurrentHashMap_get(keywords, string);

  if (RT_isNil(retVal)) {
    retain(string);
    pthread_mutex_lock(&intern_mutex);
    /* interning */
    RTValue retVal2 = ConcurrentHashMap_get(keywords, string);
    if (RT_isKeyword(retVal)) {
      pthread_mutex_unlock(&intern_mutex);
      release(string);
      return retVal2;
    }      
    
    RTValue new = RT_boxKeyword(
        atomic_fetch_add_explicit(&minUnusedKeyword, 1, memory_order_relaxed));

    retain(string);
    ConcurrentHashMap_assoc(keywordsInverted, new, string);
    ConcurrentHashMap_assoc(keywords, string, new);
    pthread_mutex_unlock(&intern_mutex);
    return new;
  }
  release(string);
  return retVal;
}

/* mem done */
String *Keyword_toString(uint32_t self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get(keywordsInverted, RT_boxKeyword(self));
  assert(!RT_isNil(retVal) && "Internal error: Keyword was not interned before printing.");
  String *result = String_concat(colon, toString(retVal));
  return result;
}


