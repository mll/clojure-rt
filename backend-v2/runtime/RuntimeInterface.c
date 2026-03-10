#include "RuntimeInterface.h"
#include "ConcurrentHashMap.h"
#include "Keyword.h"
#include "PersistentVector.h"
#include "Symbol.h"
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
  PersistentVector_cleanup();
}

void printReferenceCounts() {
  printf("Ref counters: ");
  for (unsigned char i = integerType; i <= persistentArrayMapType; i++) {
    printf("%lu/%lu ", allocationCount[i - 1], objectCount[i - 1]);
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
