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
  struct Frame *leafFrame;       // 0
  struct FunctionMethod *method; // 1
  RTValue self;                  // 2
  RTValue variadicSeq;           // 3
  int32_t bailoutEntryIndex;     // 4
  int32_t localsCount;           // 5
  RTValue locals[];              // 6...
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

struct ClojureFunction *Function_create(uword_t methodCount, uword_t maxArity,
                                        bool once);

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
void Function_promoteToShared(ClojureFunction *self, uword_t count);

RTValue RT_invokeDynamic(RTValue funObj, RTValue *args, int32_t argCount);
FunctionMethod *Function_extractMethod(RTValue funObj, uword_t argCount);
struct FunctionMethod *RT_updateICSlot(void *slot, RTValue currentVal,
                                       uint64_t argCount);

/* Global bridges for Keywords, Maps, etc. */
extern struct FunctionMethod Global_Keyword_Method_1;
extern struct FunctionMethod Global_Keyword_Method_2;
extern struct FunctionMethod Global_Map_Method_1;
extern struct FunctionMethod Global_Map_Method_2;
extern struct FunctionMethod Global_Vector_Method_1;
extern struct FunctionMethod Global_Vector_Method_2;
extern struct FunctionMethod Global_Set_Method_1;

RTValue RT_packVariadic(uword_t argCount, RTValue *args, uword_t fixedArity);

#ifdef __cplusplus
}
#endif

#endif
