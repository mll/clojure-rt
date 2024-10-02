#ifndef RT_FUNCTION
#define RT_FUNCTION
#include "String.h"
#include "defines.h"

#define INVOKATION_CACHE_SIZE 3

struct InvokationCache {
  uint64_t signature[3];
  uint64_t packed;

  unsigned char returnType;
  void *fptr;
};

typedef struct InvokationCache InvokationCache;

struct FunctionMethod  {
  unsigned char index;
  uint64_t fixedArity;
  uint64_t isVariadic;
  uint64_t closedOversCount;
  char *loopId;
  void **closedOvers;
  struct InvokationCache invokations[INVOKATION_CACHE_SIZE];
};

typedef struct FunctionMethod FunctionMethod;

struct ClojureFunction {
  uint64_t uniqueId;
  uint64_t methodCount;
  uint64_t maxArity;
  BOOL once;
  BOOL executed;
  struct FunctionMethod methods[];
};

typedef struct ClojureFunction ClojureFunction;

struct ClojureFunction* Function_create(uint64_t methodCount, uint64_t uniqueId, uint64_t maxArity, BOOL once);

void Function_fillMethod(struct ClojureFunction *self, uint64_t position, uint64_t index, uint64_t fixedArity,  BOOL isVariadic, char *loopId, int64_t closedOversCount, ...);

BOOL Function_validCallWithArgCount(ClojureFunction *self, uint64_t argCount);
BOOL Function_equals(ClojureFunction *self, ClojureFunction *other);
uint64_t Function_hash(ClojureFunction *self);
String *Function_toString(ClojureFunction *self); 
void Function_destroy(ClojureFunction *self);
void Function_cleanupOnce(ClojureFunction *self);

#endif
