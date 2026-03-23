#ifndef RT_INSTANCE_CALL_STUB_H
#define RT_INSTANCE_CALL_STUB_H

#include "../runtime/RTValue.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../runtime/word.h"

typedef struct alignas(K_WORD_SIZE * 2) {
    word_t key;
    void* value;
} InlineCache;

/**
 * The "Slow Path Bouncer" for dynamic instance calls.
 * This is called when an Inline Cache misses or is empty.
 * 
 * @param slot           Pointer to the InlineCache struct in the JITed module.
 * @param methodName     The name of the method being called.
 * @param argCount       Number of arguments (excluding instance).
 * @param args           Array of RTValue arguments (including instance as first arg).
 * @param boxedMask      Bitmask indicating which arguments are boxed (1) or unboxed (0).
 * @param compilerState  Pointer to the ThreadsafeCompilerState that created the code.
 * @return               The result of the call as a void*.
 */
__attribute__((visibility("default")))
void* InstanceCallSlowPath(
    void* slot, 
    const char* methodName, 
    int32_t argCount, 
    RTValue* args, 
    uint64_t boxedMask, 
    void* jitEngine);

#ifdef __cplusplus
}
#endif

#endif // RT_INSTANCE_CALL_STUB_H
