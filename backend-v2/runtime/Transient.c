#include "Transient.h"
#include <stdatomic.h>

#define PERSISTENT 0

uint64_t getTransientID() {
  static volatile atomic_uint_fast64_t transientID = 1;
  return atomic_fetch_add_explicit(&transientID, 1, memory_order_relaxed);
}

void assert_transient(uint64_t transientID) {
  // TODO: Runtime exception if transient == PERSISTENT
}

void assert_persistent(uint64_t transientID) {
  // TODO: Runtime exception if transient != PERSISTENT
}
