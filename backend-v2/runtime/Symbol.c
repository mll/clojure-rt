#include "Object.h"
#include "Symbol.h"
#include "RTValue.h"
#include "String.h"
#include "ConcurrentHashMap.h"
#include <pthread.h>

extern ConcurrentHashMap *symbols;
extern ConcurrentHashMap *symbolsInverted;

static pthread_mutex_t intern_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Atomic uint32_t minUnusedSymbol = 0;

/* mem done */
RTValue Symbol_create(String *string) {
  retain(string);  
  RTValue retVal = ConcurrentHashMap_get(symbols, string);

  if (RT_isNil(retVal)) {
    retain(string);
    pthread_mutex_lock(&intern_mutex);
    /* interning */
    RTValue retVal2 = ConcurrentHashMap_get(symbols, string);
    if (RT_isSymbol(retVal)) {
      pthread_mutex_unlock(&intern_mutex);
      release(string);
      return retVal2;
    }      
    
    RTValue new = RT_boxSymbol(
        atomic_fetch_add_explicit(&minUnusedSymbol, 1, memory_order_relaxed));

    retain(string);
    ConcurrentHashMap_assoc(symbolsInverted, new, string);
    ConcurrentHashMap_assoc(symbols, string, new);
    pthread_mutex_unlock(&intern_mutex);
    return new;
  }
  release(string);
  return retVal;
}

/* mem done */
String *Symbol_toString(uint32_t self) {
  String *colon = String_create(":");
  RTValue retVal = ConcurrentHashMap_get(symbolsInverted, RT_boxSymbol(self));
  assert(!RT_isNil(retVal) && "Internal error: Symbol was not interned before printing.");
  String *result = String_concat(colon, toString(retVal));
  return result;
}





