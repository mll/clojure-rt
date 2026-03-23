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
