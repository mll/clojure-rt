#include "../runtime/JITSafety.h"
#include "../jit/JITEngine.h"
 
extern "C" {
 
void JITEngine_slowPath_enter(void *engine, rt_jt_epoch_t *epochPtr) {
  if (engine)
    static_cast<rt::JITEngine *>(engine)->registerThread(epochPtr);
}
 
void JITEngine_slowPath_leave(void *engine) {
  if (engine)
    static_cast<rt::JITEngine *>(engine)->unregisterThread();
}
 
}
