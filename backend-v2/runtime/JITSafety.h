#ifndef RT_JT_SAFETY_H
#define RT_JT_SAFETY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t rt_jt_epoch_t;

// Safety state tracked per JIT instance on the host
typedef struct {
    int depth;
    rt_jt_epoch_t threadLocalEpoch;
} rt_jt_SafetyState;

#if defined(COMPILING_RUNTIME_BITCODE) && !defined(__APPLE__)
static inline void* JITEngine_getThreadPointer() {
#if defined(__x86_64__)
    void* ptr;
    __asm__("movq %%fs:0, %0" : "=r"(ptr));
    return ptr;
#elif defined(__aarch64__)
    void* ptr;
    __asm__("mrs %0, tpidr_el0" : "=r"(ptr));
    return ptr;
#else
    return (void*)0;
#endif
}
extern uintptr_t host_jit_safety_state_offset;
#define host_jit_safety_state (*(rt_jt_SafetyState*)((char*)JITEngine_getThreadPointer() + host_jit_safety_state_offset))
#else
extern _Thread_local rt_jt_SafetyState host_jit_safety_state;
#endif

// Fast-path safety section signals (inlineable)
void JITEngine_enterSafeSection(void *engine);
void JITEngine_leaveSafeSection(void *engine);

// Slow-path signals (implemented in Host C++)
void JITEngine_slowPath_enter(void *engine, rt_jt_epoch_t *epochPtr);
void JITEngine_slowPath_leave(void *engine);

#ifdef __cplusplus
}
#endif

#endif
