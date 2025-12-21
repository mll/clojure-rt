#ifndef RT_INTERFACE
#define RT_INTERFACE
#include "Object.h"
#include "RTValue.h"

void Interface_initialise();


inline bool logicalValue(RTValue self) {
  if(RT_isNil(self)) return false;
  if(RT_isBool(self)) return RT_unboxBool(self);
  return true;
}

inline void logException(const char *description) {
  printf("%s\n", description);
  logBacktrace();
  exit(1);
}

inline void logType(const objectType ll) {
  printf("Type: %d\n", ll);
}

inline void logText(const char *text) {
  printf("Log: %s\n", text);
}

inline bool unboxedEqualsInteger(RTValue left, int32_t right) {
  if(!RT_isInt32(left)) return false;
  return RT_unboxInt32(left) == right;
}

inline bool unboxedEqualsDouble(RTValue left, double right) {
  if(!RT_isDouble(left)) return false;
  return RT_unboxDouble(left) == right;  
}

inline bool unboxedEqualsBoolean(RTValue left, bool right) {
  if(!RT_isBool(left)) return false;
  return RT_unboxBool(left) == right;    
}

void printReferenceCounts();

void **packPointerArgs(uint64_t count, ...);

#endif

