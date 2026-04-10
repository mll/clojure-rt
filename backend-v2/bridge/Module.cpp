#include "Module.h"
#include "jit/JITEngine.h"

extern "C" __attribute__((visibility("default"))) void
destroyModule(void *module, void *jitEngine) {
  rt::JITEngine *engine = (rt::JITEngine *)jitEngine;
  rt::JITEngine::ModuleTracker *tracker =
      (rt::JITEngine::ModuleTracker *)module;
  for (RTValue v : tracker->constants)
    ::release(v);
  engine->removeModule(tracker->tracker);
  delete tracker;
}
