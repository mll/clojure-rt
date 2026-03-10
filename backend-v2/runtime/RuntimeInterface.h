#ifndef RT_RUNTIME_INTERFACE
#define RT_RUNTIME_INTERFACE

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "Object.h"
#include "RTValue.h"

void RuntimeInterface_initialise();
void RuntimeInterface_cleanup();

typedef struct {
  uword_t counts[256];
  uint32_t internedKeywords;
  uint32_t internedSymbols;
} MemoryState;

void captureMemoryState(MemoryState *state);

#ifdef __cplusplus
}
#endif

inline bool logicalValue(RTValue self) {
  if (RT_isNil(self))
    return false;
  if (RT_isBool(self))
    return RT_unboxBool(self);
  return true;
}

#ifdef __cplusplus
extern "C" {
#endif
void logBacktrace();
#ifdef __cplusplus
}
#endif

inline void logException(const char *description) {
  printf("%s\n", description);
  logBacktrace();
  exit(1);
}

inline void logType(const objectType ll) { printf("Type: %d\n", ll); }

inline void logText(const char *text) { printf("Log: %s\n", text); }

inline bool unboxedEqualsInteger(RTValue left, int32_t right) {
  if (!RT_isInt32(left))
    return false;
  return RT_unboxInt32(left) == right;
}

inline bool unboxedEqualsDouble(RTValue left, double right) {
  if (!RT_isDouble(left))
    return false;
  return RT_unboxDouble(left) == right;
}

inline bool unboxedEqualsBoolean(RTValue left, bool right) {
  if (!RT_isBool(left))
    return false;
  return RT_unboxBool(left) == right;
}

#ifdef __cplusplus
extern "C" {
#endif

void printReferenceCounts();

void **packPointerArgs(uword_t count, ...);
RTValue *packValueArgs(uword_t count, ...);

#ifdef __cplusplus
}
#endif

#endif
