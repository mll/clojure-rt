#include "Object.h"
#include "Symbol.h"
#include "RTValue.h"
#include "String.h"
#include "ConcurrentHashMap.h"
#include <pthread.h>

extern ConcurrentHashMap *symbols;
extern ConcurrentHashMap *symbolsInverted;

static pthread_mutex_t intern_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic uint32_t minUnusedSymbol = 1;

/* mem done */
RTValue Symbol_create(String *string) {
  RTValue stringVal = RT_boxPtr(string);
  Ptr_retain(string);  
  RTValue retVal = ConcurrentHashMap_get(symbols, stringVal);

  if (RT_isNil(retVal)) {
    Ptr_retain(string);
    pthread_mutex_lock(&intern_mutex);
    /* interning */
    RTValue retVal2 = ConcurrentHashMap_get(symbols, stringVal);
    if (RT_isSymbol(retVal)) {
      pthread_mutex_unlock(&intern_mutex);
      Ptr_release(string);
      return retVal2;
    }      
    
    RTValue new = RT_boxSymbol(
        atomic_fetch_add_explicit(&minUnusedSymbol, 1, memory_order_relaxed));

    Ptr_retain(string);
    ConcurrentHashMap_assoc(symbolsInverted, new, stringVal);
    ConcurrentHashMap_assoc(symbols, stringVal, new);
    pthread_mutex_unlock(&intern_mutex);
    return new;
  }
  Ptr_release(string);
  return retVal;
}

/* mem done */
String *Symbol_toString(RTValue self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get(symbolsInverted, self);
  assert(!RT_isNil(retVal) && "Internal error: Symbol was not interned before printing.");
  String *result = String_concat(colon, toString(retVal));
  return result;
}





