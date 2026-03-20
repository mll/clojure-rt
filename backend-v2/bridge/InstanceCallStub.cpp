#include "InstanceCallStub.h"
#include "../jit/JITEngine.h"
#include "../runtime/Object.h"
#include "Exceptions.h"
#include <iostream>
#include <vector>

using namespace rt;

void releaseInstanceStubArgs(int32_t argCount, RTValue *args) {
  for (int i = 0; i < argCount + 1; i++) {
    release(args[i]);
  }
}

/**
 * The "Slow Path Bouncer" for dynamic instance calls.
 *
 * This function is called when an Inline Cache misses. It performs the
 * following:
 * 1. Resolves the actual runtime type of the instance.
 * 2. Triggers the JIT compilation of a specialized bridge stub for this
 * method/type.
 * 3. Updates the Inline Cache (IC) slot with the new bridge address and type
 * key.
 * 4. Executes the bridge and returns the result.
 */
extern "C" __attribute__((visibility("default"))) void *
InstanceCallSlowPath(void *slot, const char *methodName, int32_t argCount,
                     RTValue *args, uint64_t boxedMask, void *jitEngine) {

  if (!jitEngine) {
    releaseInstanceStubArgs(argCount, args);
    throwInternalInconsistencyException(
        "InstanceCallSlowPath: jitEngine is null");
  }
  if (!slot) {
    releaseInstanceStubArgs(argCount, args);
    throwInternalInconsistencyException(
        "InstanceCallSlowPath: IC slot is null");
  }
  if (argCount >= 64) {
    releaseInstanceStubArgs(argCount, args);
    throwInternalInconsistencyException(
        "InstanceCallSlowPath: argCount >= 64 is not supported due to "
        "boxedMask limitation");
  }

  JITEngine *engine = static_cast<JITEngine *>(jitEngine);
  InlineCache *ic = static_cast<InlineCache *>(slot);
 
  JITEngine::SafetySection safety(*engine);
 
  // 1. Determine the actual type of the instance at runtime
  RTValue instance = args[0];
  objectType instanceType = getType(instance);

  // 2. Prepare the argument types for JIT specialization
  std::vector<ObjectTypeSet> argTypes;
  for (int i = 0; i < argCount; i++) {
    // bit i of boxedMask corresponds to args[i+1] (the i-th method argument)
    bool isBoxed = (boxedMask >> i) & 1;
    if (isBoxed) {
      argTypes.push_back(ObjectTypeSet::dynamicType());
    } else {
      // For unboxed arguments, we specialize to the actual runtime type
      argTypes.push_back(ObjectTypeSet(getType(args[i + 1])));
    }
  }

  // 3. Trigger JIT compilation of a specialized bridge stub
  auto future = engine->compileInstanceCallBridge(
      methodName ? methodName : "unknown", ObjectTypeSet(instanceType),
      argTypes, slot, llvm::OptimizationLevel::O0, true);

  try {
    // Block until the JIT compilation is finished and get the executable
    // address
    llvm::orc::ExecutorAddr bridgeAddr = future.get();
    void *bridgePtr = bridgeAddr.toPtr<void *>();
    // 4. Update the Inline Cache for subsequent calls (atomically)
    InlineCache newCache = {(word_t)instanceType, bridgePtr};
    if constexpr (K_WORD_SIZE == 8) {
        __atomic_store((unsigned __int128 *)ic, (unsigned __int128 *)&newCache,
                       __ATOMIC_RELEASE);
    } else {
        __atomic_store((uint64_t *)ic, (uint64_t *)&newCache, __ATOMIC_RELEASE);
    }
    engine->commit(methodName);

    // 5. Return the bridge pointer. The caller will jump to it.
    return bridgePtr;
  } catch (...) {
    releaseInstanceStubArgs(argCount, args);
    throw;
  }
}
