#include "ClassLookup.h"
#include "../jit/JITEngine.h"
#include "Exceptions.h"
#include "runtime/Class.h"
#include <string>

using namespace rt;

extern "C" __attribute__((visibility("default"))) ::Class *
ClassLookupByName(const char *className, void *jitEngine) {
  if (!jitEngine) {
    throwInternalInconsistencyException("ClassLookup: jitEngine is null");
  }
  JITEngine *engine = static_cast<JITEngine *>(jitEngine);
  Class *cls = engine->threadsafeState.classRegistry.getCurrent(className);
  if (!cls) {
    cls = engine->threadsafeState.protocolRegistry.getCurrent(className);
  }
  if (!cls) {
    throwInternalInconsistencyException("ClassLookup: class/protocol not found: " +
                                        std::string(className));
  }
  return cls;
}

extern "C" __attribute__((visibility("default"))) ::Class *
ClassLookupByRegisterId(int32_t registerId, void *jitEngine) {
  if (!jitEngine) {
    throwInternalInconsistencyException("ClassLookup: jitEngine is null");
  }
  JITEngine *engine = static_cast<JITEngine *>(jitEngine);
  Class *cls = engine->threadsafeState.classRegistry.getCurrent(registerId);
  if (!cls) {
    cls = engine->threadsafeState.protocolRegistry.getCurrent(registerId);
  }
  if (!cls) {
    return nullptr;
  }
  return cls;
}
