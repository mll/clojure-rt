#ifndef RT_MODULE_H
#define RT_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default"))) void
destroyModule(void *module, void *jitEngine);

#ifdef __cplusplus
}
#endif

#endif // RT_MODULE_H
