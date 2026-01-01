#ifndef RT_RUNTIME_HEADERS
#define RT_RUNTIME_HEADERS

#include <gmp.h>

extern "C" {
#ifdef __cplusplus
  #if defined(__GNUC__) || defined(__clang__)
    #define restrict __restrict__
  #elif defined(_MSC_VER)
    #define restrict __restrict
  #else
    #define restrict
  #endif
#endif

#include "runtime/RTValue.h"
#include "runtime/word.h"
#include "runtime/ObjectProto.h"
#include "runtime/Object.h"
#include "runtime/String.h"
#include "runtime/PersistentVector.h"
#include "runtime/PersistentList.h"
#include "runtime/PersistentArrayMap.h"
#include "runtime/Keyword.h"
#include "runtime/Symbol.h"
#include "runtime/BigInteger.h"
#include "runtime/Ratio.h"
}

#endif


