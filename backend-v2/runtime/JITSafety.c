#include "JITSafety.h"
#include <stdbool.h>
#include <stdio.h>

_Thread_local rt_jt_SafetyState host_jit_safety_state;

void JITEngine_enterSafeSection(void *engine) {
  // Fast path: increment depth
  if (host_jit_safety_state.depth++ == 0) {
    // First entry: go to slow path in Host C++
    JITEngine_slowPath_enter(engine, &host_jit_safety_state.threadLocalEpoch);
  }
}

void JITEngine_leaveSafeSection(void *engine) {
  // Fast path: decrement depth
  if (--host_jit_safety_state.depth == 0) {
    // Last exit: go to slow path in Host C++
    JITEngine_slowPath_leave(engine);
  }
}
