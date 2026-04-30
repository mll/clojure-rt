#include "Symbol.h"
#include "ConcurrentHashMap.h"
#include "Object.h"
#include "RTValue.h"
#include "String.h"
#include <pthread.h>

extern ConcurrentHashMap *symbols;

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

    Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
    Object_create(&sym->header, symbolType);
    sym->name = string;
    sym->ns = NULL;
    sym->metadata = RT_boxNil();
    Ptr_retain(string); // Symbol holds a ref to the name

    RTValue newSymVal = RT_boxSymbol(sym);

    /* Need 2 refs for assoc call (consumes stringVal as key and newSymVal as value).
       We currently hold 1 (caller's) for string, and 1 for sym. 
       So retain sym once more. */
    retain(newSymVal);
    Ptr_retain(string); 
    ConcurrentHashMap_assoc_preservesSelf(symbols, stringVal, newSymVal,
                                          false); /* consumes 1 */
    
    atomic_fetch_add_explicit(&minUnusedSymbol, 1, memory_order_relaxed);
    pthread_mutex_unlock(&intern_mutex);
    
    Ptr_release(string); /* consume caller's ref */
    return newSymVal;
  }
  Ptr_release(string); /* consume caller's ref */
  return retVal;
}

/* mem done - consumes string, consumes meta */
RTValue Symbol_createWithMeta(String *string, RTValue meta) {
  Symbol *sym = (Symbol *)allocate(sizeof(Symbol));
  Object_create(&sym->header, symbolType);
  sym->name = string;
  sym->ns = NULL;
  sym->metadata = meta;
  Ptr_retain(string); // Symbol holds a ref to the name (consumes caller's ref later)
  
  Ptr_release(string); // consume caller's ref
  return RT_boxSymbol(sym);
}

String *Symbol_getName(Symbol *self) {
  return self->name;
}

void Symbol_destroy(Symbol *self) {
  Ptr_release(self->name);
  if (self->ns) Ptr_release(self->ns);
  release(self->metadata);
}

bool Symbol_equals(Symbol *self, Symbol *other) {
  if (self == other) return true;
  return String_equals(self->name, other->name);
}

uword_t Symbol_hash(Symbol *self) {
  return String_hash(self->name);
}

/* mem done */
String *Symbol_toString(RTValue self) {
  Symbol *sym = (Symbol *)RT_unboxSymbol(self);
  String *res = sym->name;
  Ptr_retain(res);
  return res;
}

uint32_t Symbol_getInternCount() {
  return atomic_load_explicit(&minUnusedSymbol, memory_order_relaxed) - 1;
}

void Symbol_resetInterns() {
  atomic_store_explicit(&minUnusedSymbol, 1, memory_order_relaxed);
}
