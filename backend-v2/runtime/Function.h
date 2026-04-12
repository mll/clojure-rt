#ifndef RT_FUNCTION
#define RT_FUNCTION

#ifdef __cplusplus
extern "C" {
#endif

#include "RTValue.h"
#include "String.h"
#include "defines.h"

#define INVOKATION_CACHE_SIZE 3

typedef struct InvokationCache InvokationCache;

struct FunctionMethod {
  unsigned char index;
  unsigned char isVariadic;  
  unsigned char fixedArity;
  unsigned char closedOversCount;
  void *baselineImplementation;
  char *loopId;
  RTValue *closedOvers;
};

typedef struct Frame {
  struct Frame *leafFrame;
  struct FunctionMethod *method;
  RTValue variadicSeq;
  int32_t bailoutEntryIndex;
  int32_t localsCount;
  RTValue locals[];
} Frame;

typedef struct FunctionMethod FunctionMethod;

typedef struct Object Object;

struct ClojureFunction {
  Object super;
  bool once;
  bool executed;
  uword_t methodCount;
  uword_t maxArity;
  struct FunctionMethod methods[];
};

typedef struct ClojureFunction ClojureFunction;

struct ClojureFunction *Function_create(uword_t methodCount, uword_t maxArity, bool once);

void Function_fillMethod(struct ClojureFunction *self, uword_t position,
                         uword_t index, uword_t fixedArity, bool isVariadic,
                         void *implementation, char *loopId,
                         word_t closedOversCount, ...);

bool Function_validCallWithArgCount(ClojureFunction *self, uword_t argCount);
bool Function_equals(ClojureFunction *self, ClojureFunction *other);
uword_t Function_hash(ClojureFunction *self);
String *Function_toString(ClojureFunction *self);
void Function_destroy(ClojureFunction *self);
void Function_cleanupOnce(ClojureFunction *self);
RTValue RT_invokeDynamic(RTValue funObj, RTValue *args, int32_t argCount);

#ifdef __cplusplus
}
#endif

#endif
