#include "Module.h"
#include "jit/JITEngine.h"
#include "runtime/Object.h"
#include "runtime/RTValue.h"
#include "runtime/String.h"

extern "C" __attribute__((visibility("default"))) void
destroyModule(void *module, void *jitEngine) {
  rt::JITEngine *engine = (rt::JITEngine *)jitEngine;
  rt::JITEngine::ModuleTracker *tracker =
      (rt::JITEngine::ModuleTracker *)module;
  for (RTValue v : tracker->constants) {
    ::release(v);
  }
  for (void *slot : tracker->icSlotAddresses) {
    RTValue key = 0;
    // Atomic exchange to 'steal' the pointer and zero the slot.
    // This ensures only one module/thread can release the key.
    __atomic_exchange((uint64_t *)slot, &key, &key, __ATOMIC_RELAXED);
    if (key != 0) {
      ::release(key);
    }
  }
  engine->removeModule(tracker->tracker);
  delete tracker;
}
