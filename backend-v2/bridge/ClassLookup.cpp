#include "ClassLookup.h"
#include "../jit/JITEngine.h"
#include "Exceptions.h"
#include <string>

using namespace rt;

extern "C" __attribute__((visibility("default"))) ::Class *
ClassLookup(const char *className, void *jitEngine) {
  if (!jitEngine) {
    throwInternalInconsistencyException("ClassLookup: jitEngine is null");
  }
  JITEngine *engine = static_cast<JITEngine *>(jitEngine);
  Class *cls = engine->threadsafeState.classRegistry.getCurrent(className);
  if (!cls) {
    throwInternalInconsistencyException("ClassLookup: class not found: " +
                                        std::string(className));
  }
  return cls;
}
