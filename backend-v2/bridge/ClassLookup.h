#ifndef RT_CLASS_LOOKUP_H
#define RT_CLASS_LOOKUP_H

#ifdef __cplusplus
extern "C" {
#endif

struct Class;

__attribute__((visibility("default"))) ::Class *
ClassLookup(const char *className, void *jitEngine);

#ifdef __cplusplus
}
#endif

#endif // RT_CLASS_LOOKUP_H
