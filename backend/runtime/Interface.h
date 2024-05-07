#ifndef RT_INTERFACE
#define RT_INTERFACE
#include "Object.h"

void Interface_initialise();


inline BOOL logicalValue(void * restrict self) {
  Object *o = super(self);
  objectType type = o->type;
  if(type == nilType) return FALSE;
  if(type == booleanType) return ((Boolean *)self)->value;
  return TRUE;
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

inline BOOL unboxedEqualsInteger(void *left, uint64_t right) {
  Object *o = super(left);
  if(o->type != integerType) return FALSE;
  Integer *i = (Integer *) left;
  return i->value == right;
}

inline BOOL unboxedEqualsDouble(void *left, double right) {
  Object *o = super(left);
  if(o->type != doubleType) return FALSE;
  Double *i = (Double *) left;
  return i->value == right;
}

inline BOOL unboxedEqualsBoolean(void *left, BOOL right) {
  Object *o = super(left);  
  if(o->type != booleanType) return FALSE;
  Boolean *i = (Boolean *) left;
  return i->value == right;
}

void printReferenceCounts();

void **packPointerArgs(uint64_t count, ...);

#endif

