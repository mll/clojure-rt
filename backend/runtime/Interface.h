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


#endif

