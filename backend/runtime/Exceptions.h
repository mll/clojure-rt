#ifndef RT_RUNTIME_EXCEPTIONS_H
#define RT_RUNTIME_EXCEPTIONS_H

#include "RTValue.h"
#include "word.h"

#ifdef __cplusplus
extern "C" {
#include "runtime/String.h"

#endif

// Base throw function compatible with LanguageException
[[noreturn]] void throwLanguageException_C(const char *name, RTValue message,
                                           RTValue payload);
[[noreturn]] void throwException_C(RTValue exceptionBoxed);

// Standard Clojure-like exceptions
[[noreturn]] void throwArityException_C(int expected, int actual);
[[noreturn]] void throwIllegalArgumentException_C(const char *message);
[[noreturn]] void throwIllegalStateException_C(const char *message);
[[noreturn]] void throwUnsupportedOperationException_C(const char *message);
[[noreturn]] void throwArithmeticException_C(const char *message);
[[noreturn]] void throwIndexOutOfBoundsException_C(uword_t index,
                                                   uword_t count);
[[noreturn]] void throwInternalInconsistencyException_C(String *errorMessage);

#ifdef __cplusplus
}
#endif

#endif
