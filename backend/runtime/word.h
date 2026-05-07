#ifndef WORD_H
#define WORD_H

#include <stdint.h>
#include <stddef.h>

// The fundamental unit of data in clojurert.
// intptr_t is guaranteed to be the same size as a pointer.
typedef intptr_t  word_t;
typedef uintptr_t uword_t;

// Alignment constant for the target architecture.
// In C, 'const' isn't a true compile-time constant for array sizes 
// in all contexts, so we use a macro or an enum for the size.
#define K_WORD_SIZE sizeof(word_t)

#endif

