#include "Interface.h"
#include "ConcurrentHashMap.h"
#include <stdio.h>
#include <stdarg.h>

ConcurrentHashMap *keywords = NULL;
ConcurrentHashMap *vars = NULL;

extern BOOL logicalValue(void * restrict self);
extern void logException(const char *description);
extern BOOL unboxedEqualsInteger(void *left, int64_t right);
extern BOOL unboxedEqualsDouble(void *left, double right);
extern BOOL unboxedEqualsBoolean(void *left, BOOL right);
extern void logType(const objectType ll);
extern void logText(const char *text);
extern void printReferenceCounts();

void Interface_initialise() {
  keywords = ConcurrentHashMap_create(10); // 2^10
  vars = ConcurrentHashMap_create(10); // 2^10
}

void printReferenceCounts() {
  printf("Ref counters: ");  
  for(int i= integerType; i<=persistentArrayMapType; i++) {
    printf("%llu/%llu ", allocationCount[i-1], objectCount[i-1]);    
  }
  printf("\n");
}


void logBacktrace() {
 void *array[1000];
  char **strings;
  int size, i;

  size = backtrace (array, 1000);
  strings = backtrace_symbols (array, size);
  if (strings != NULL)
  {

    printf ("Obtained %d stack frames:\n", size);
    for (i = 0; i < size; i++)
      printf ("%s\n", strings[i]);
  }

  free (strings);
}

void **packPointerArgs(uint64_t count, ...) {
  if (!count) return NULL;
  void **ptr = allocate(sizeof(void *) * count);
  va_list args;
  va_start(args, count);
  for (int i = 0; i < count; ++i) {
    ptr[i] = va_arg(args, void *);
  }
  va_end(args);
  return ptr;
}

