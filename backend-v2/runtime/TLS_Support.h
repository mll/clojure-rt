#ifndef RT_TLS_SUPPORT_H
#define RT_TLS_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

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
#endif

#ifdef __cplusplus
}
#endif

#endif // RT_TLS_SUPPORT_H
