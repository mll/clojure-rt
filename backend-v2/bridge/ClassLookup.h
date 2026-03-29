#ifndef RT_CLASS_LOOKUP_H
#define RT_CLASS_LOOKUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct Class;

__attribute__((visibility("default"))) ::Class *
ClassLookupByName(const char *className, void *jitEngine);
__attribute__((visibility("default"))) ::Class *
ClassLookupByRegisterId(int32_t registerId, void *jitEngine);

#ifdef __cplusplus
}
#endif

#endif // RT_CLASS_LOOKUP_H
