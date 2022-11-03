#include "Interface.h"
#include "ConcurrentHashMap.h"

ConcurrentHashMap *keywords = NULL;
ConcurrentHashMap *vars = NULL;

extern BOOL logicalValue(void * restrict self);
extern void logException(const char *description);

void Interface_initialise() {
  keywords = ConcurrentHashMap_create(10);
  vars = ConcurrentHashMap_create(10);
}
