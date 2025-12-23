#ifndef RT_FUNCTION
#define RT_FUNCTION
#include "String.h"
#include "defines.h"
#include "RTValue.h"

#define INVOKATION_CACHE_SIZE 3

struct InvokationCache {
  uword_t signature[3];
  uword_t packed;

  unsigned char returnType;
  void *fptr;
};

typedef struct InvokationCache InvokationCache;

struct FunctionMethod  {
  unsigned char index;
  uword_t fixedArity;
  uword_t isVariadic;
  uword_t closedOversCount;
  char *loopId;
  RTValue *closedOvers;
  struct InvokationCache invokations[INVOKATION_CACHE_SIZE];
};

typedef struct FunctionMethod FunctionMethod;

typedef struct Object Object;

struct ClojureFunction {
  Object super;
  bool once;
  bool executed;
  uword_t uniqueId;
  uword_t methodCount;
  uword_t maxArity;
  struct FunctionMethod methods[];
};

typedef struct ClojureFunction ClojureFunction;

struct ClojureFunction* Function_create(uword_t methodCount, uword_t uniqueId, uword_t maxArity, bool once);

void Function_fillMethod(struct ClojureFunction *self, uword_t position, uword_t index, uword_t fixedArity,  bool isVariadic, char *loopId, word_t closedOversCount, ...);

bool Function_validCallWithArgCount(ClojureFunction *self, uword_t argCount);
bool Function_equals(ClojureFunction *self, ClojureFunction *other);
uword_t Function_hash(ClojureFunction *self);
String *Function_toString(ClojureFunction *self); 
void Function_destroy(ClojureFunction *self);
void Function_cleanupOnce(ClojureFunction *self);

#endif
