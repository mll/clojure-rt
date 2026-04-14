#include "Module.h"
#include "jit/JITEngine.h"

extern "C" __attribute__((visibility("default"))) void
destroyModule(void *module, void *jitEngine) {
  rt::JITEngine *engine = (rt::JITEngine *)jitEngine;
  rt::JITEngine::ModuleTracker *tracker =
      (rt::JITEngine::ModuleTracker *)module;
  for (RTValue v : tracker->constants)
    ::release(v);
  for (void *slot : tracker->icSlotAddresses) {
    RTValue key;
    // Atomic load key from slot (first 64 bits)
    __atomic_load((uint64_t *)slot, &key, __ATOMIC_RELAXED);
    if (key != 0) {
      ::release(key);
    }
  }
  engine->removeModule(tracker->tracker);
  delete tracker;
}
