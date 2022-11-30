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
  void *array[1000];
  char **strings;
  int size, i;

  printf("%s\n", description);

  size = backtrace (array, 1000);
  strings = backtrace_symbols (array, size);
  if (strings != NULL)
  {

    printf ("Obtained %d stack frames:\n", size);
    for (i = 0; i < size; i++)
      printf ("%s\n", strings[i]);
  }

  free (strings);
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

#endif

