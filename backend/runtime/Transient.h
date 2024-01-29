#ifndef RT_TRANSIENT
#define RT_TRANSIENT

#include <stdatomic.h>

#define PERSISTENT 0

uint64_t getTransientID();
void assert_transient(uint64_t transientID);
void assert_persistent(uint64_t transientID);

#endif
