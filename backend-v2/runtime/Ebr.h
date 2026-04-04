#ifndef RT_EBR_H
#define RT_EBR_H

#include "RTValue.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t rt_jt_epoch_t;

/* Epoch based reclaimation */

void Ebr_init();
void Ebr_shutdown();

void Ebr_register_thread();
void Ebr_unregister_thread();

void Ebr_enter_critical();
void Ebr_leave_critical();

void autorelease(RTValue value);

#ifdef __cplusplus
}
#endif

#endif
