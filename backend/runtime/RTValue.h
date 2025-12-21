#ifndef RT_VALUE_H
#define RT_VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// The universal 64-bit container for all ClojureRT data
typedef uint64_t RTValue;

// High 16-bit Tags (Matching your LLVM ValueEncoder)
#define RT_TAG_MASK         0xFFFF000000000000ULL
#define RT_TAG_DOUBLE_START 0xFFF0000000000000ULL
#define RT_TAG_INT32        0xFFFF000000000000ULL
#define RT_TAG_PTR          0xFFFE000000000000ULL
#define RT_TAG_BOOL         0xFFFD000000000000ULL
#define RT_TAG_NULL         0xFFFC000000000000ULL
#define RT_TAG_KEYWORD      0xFFFB000000000000ULL
#define RT_TAG_SYMBOL       0xFFFB000000000000ULL

// --- Type Checking ---

static inline bool RT_isDouble(RTValue v)  { return v < RT_TAG_DOUBLE_START; }
static inline bool RT_isInt32(RTValue v)   { return (v & RT_TAG_MASK) == RT_TAG_INT32; }
static inline bool RT_isPtr(RTValue v)     { return (v & RT_TAG_MASK) == RT_TAG_PTR; }
static inline bool RT_isBool(RTValue v)    { return (v & RT_TAG_MASK) == RT_TAG_BOOL; }
static inline bool RT_isNil(RTValue v)     { return v == RT_TAG_NULL; }
static inline bool RT_isKeyword(RTValue v) { return (v & RT_TAG_MASK) == RT_TAG_KEYWORD; }
static inline bool RT_isSymbol(RTValue v) { return (v & RT_TAG_MASK) == RT_TAG_SYMBOL; }

// --- Boxing (Creating RTValues) ---

static inline RTValue RT_boxPtr(void* p) {
    // 1. Cast pointer to the machine's native unsigned word (32 or 64 bit)
    // 2. Cast that to 64-bit (zero-extends on 32-bit systems)
    // 3. Apply the 16-bit Pointer Tag
    return ((uint64_t)(uintptr_t)p) | RT_TAG_PTR;
}


/* When using memcpy with a constant size (like sizeof(double)), LLVM does not
   call the libc function.

   Instead, the LLVM optimizer recognizes this as a "bit-cast". It generates a
   single CPU instruction (usually a movq or vmovq between an FPU/XMM register and
   a General Purpose register). */

static inline RTValue RT_boxDouble(double d) {
    RTValue v;
    memcpy(&v, &d, sizeof(double));
    return v;
}

static inline RTValue RT_boxInt32(int32_t i) {
    return ((RTValue)(uint32_t)i) | RT_TAG_INT32;
}

static inline RTValue RT_boxPtr(void* p) {
    return ((RTValue)(uintptr_t)p) | RT_TAG_PTR;
}

static inline RTValue RT_boxBool(bool b) {
    return ((RTValue)(b ? 1 : 0)) | RT_TAG_BOOL;
}

static inline RTValue RT_boxNil() {
    return RT_TAG_NULL;
}

static inline RTValue RT_boxKeyword(uint32_t id) {
    return ((RTValue)id) | RT_TAG_KEYWORD;
}

static inline RTValue RT_boxSymbol(uint32_t id) {
    return ((RTValue)id) | RT_TAG_SYMBOL;
}

// --- Unboxing (Extracting Data) ---

static inline double RT_unboxDouble(RTValue v) {
    double d;
    memcpy(&d, &v, sizeof(double));
    return d;
}

static inline int32_t RT_unboxInt32(RTValue v)   { return (int32_t)(v & 0xFFFFFFFFULL); }
static inline void* RT_unboxPtr(RTValue v)     { return (void*)(uintptr_t)(v & ~RT_TAG_MASK); }
static inline bool    RT_unboxBool(RTValue v)    { return (bool)(v & 0x1ULL); }
static inline uint32_t RT_unboxKeyword(RTValue v) { return (uint32_t)(v & 0xFFFFFFFFULL); }
static inline uint32_t RT_unboxSymbol(RTValue v) { return (uint32_t)(v & 0xFFFFFFFFULL); }

#endif // RT_VALUE_H
