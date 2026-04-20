#include "RuntimeInterface.h"
#include "ConcurrentHashMap.h"
#include "Ebr.h"

#include "ObjectProto.h"
#include "PersistentArrayMap.h"
#include "PersistentVector.h"
#include "Symbol.h"
#include "Var.h"
#include <stdarg.h>
#include <stdio.h>

ConcurrentHashMap *keywords = NULL;
ConcurrentHashMap *keywordsInverted = NULL;
ConcurrentHashMap *symbols = NULL;
ConcurrentHashMap *symbolsInverted = NULL;

ConcurrentHashMap *vars = NULL;

extern bool logicalValue(RTValue self);
extern void logException(const char *description);
extern bool unboxedEqualsInteger(RTValue left, int32_t right);
extern bool unboxedEqualsDouble(RTValue left, double right);
extern bool unboxedEqualsBoolean(RTValue left, bool right);
extern void logType(const objectType ll);
extern void logText(const char *text);
extern void printReferenceCounts();
extern uint64_t avalanche_64(uint64_t h);

void RuntimeInterface_initialise() {
  if (keywords)
    return;
  Ebr_init();
  // Registering main thread.
  Ebr_register_thread();
  PersistentVector_initialise();
  PersistentArrayMap_initialise();
  PersistentList_initialise();

  keywords = ConcurrentHashMap_create(10);         // 2^10
  keywordsInverted = ConcurrentHashMap_create(10); // 2^10
  vars = ConcurrentHashMap_create(10);             // 2^10
  symbols = ConcurrentHashMap_create(10);
  symbolsInverted = ConcurrentHashMap_create(10);
}

void RuntimeInterface_cleanup() {
  if (keywords) {
    Ptr_release(keywords);
    keywords = NULL;
  }
  if (keywordsInverted) {
    Ptr_release(keywordsInverted);
    keywordsInverted = NULL;
  }
  if (symbols) {
    Ptr_release(symbols);
    symbols = NULL;
  }
  if (symbolsInverted) {
    Ptr_release(symbolsInverted);
    symbolsInverted = NULL;
  }
  if (vars) {
    Ptr_release(vars);
    vars = NULL;
  }
  PersistentList_cleanup();
  PersistentVector_cleanup();
  PersistentArrayMap_cleanup();
  // Unregistering main thread.
  Ebr_unregister_thread();
  Ebr_shutdown();
  Keyword_resetInterns();
  Symbol_resetInterns();
}

void printReferenceCounts() {
  printf("Ref counters: ");
  // Iterate through all defined types in the enum
  for (unsigned char i = stringType; i <= stringBuilderType; i++) {
    printf("%lu/%lu ", (unsigned long)allocationCount[i - 1],
           (unsigned long)objectCount[i - 1]);
  }
  printf("\n");
}

void captureMemoryState(MemoryState *state) {
  for (int i = 0; i < 256; i++) {
    state->counts[i] = allocationCount[i];
  }
  state->internedKeywords = Keyword_getInternCount();
  state->internedSymbols = Symbol_getInternCount();
}

void logBacktrace() {
  void *array[1000];
  char **strings;
  int size, i;

  size = backtrace(array, 1000);
  strings = backtrace_symbols(array, size);
  if (strings != NULL) {

    printf("Obtained %d stack frames:\n", size);
    for (i = 0; i < size; i++)
      printf("%s\n", strings[i]);
  }

  free(strings);
}

void **packPointerArgs(uword_t count, ...) {
  if (!count)
    return NULL;
  void **ptr = allocate(sizeof(void *) * count);
  va_list args;
  va_start(args, count);
  for (uword_t i = 0; i < count; ++i) {
    ptr[i] = va_arg(args, void *);
  }
  va_end(args);
  return ptr;
}

RTValue *packValueArgs(uword_t count, ...) {
  if (!count)
    return NULL;
  RTValue *ptr = allocate(sizeof(RTValue) * count);
  va_list args;
  va_start(args, count);
  for (uword_t i = 0; i < count; ++i) {
    ptr[i] = va_arg(args, RTValue);
  }
  va_end(args);
  return ptr;
}
