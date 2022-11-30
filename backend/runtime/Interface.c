#include "Interface.h"
#include "ConcurrentHashMap.h"
#include <stdio.h>

ConcurrentHashMap *keywords = NULL;
ConcurrentHashMap *vars = NULL;

extern BOOL logicalValue(void * restrict self);
extern void logException(const char *description);
extern BOOL unboxedEqualsInteger(void *left, uint64_t right);
extern BOOL unboxedEqualsDouble(void *left, double right);
extern BOOL unboxedEqualsBoolean(void *left, BOOL right);
extern void logType(const objectType ll);
extern void logText(const char *text);
extern void printReferenceCounts();

void Interface_initialise() {
  keywords = ConcurrentHashMap_create(10);
  vars = ConcurrentHashMap_create(10);
}

void printReferenceCounts() {
  printf("Ref counters: ");  
  for(int i= integerType; i<=persistentArrayMapType; i++) {
    printf("%llu ", allocationCount[i-1]);    
  }
  printf("\n");
}


