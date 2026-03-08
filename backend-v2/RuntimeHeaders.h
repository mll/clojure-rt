#ifndef RT_RUNTIME_HEADERS
#define RT_RUNTIME_HEADERS

#include <gmp.h>

#ifdef __cplusplus
#include <atomic>
#include <memory>
extern "C" {
#endif
#ifdef __cplusplus
#if defined(__GNUC__) || defined(__clang__)
#define restrict __restrict__
#elif defined(_MSC_VER)
#define restrict __restrict
#else
#define restrict
#endif
#endif

#include "runtime/BigInteger.h"
#include "runtime/Keyword.h"
#include "runtime/Object.h"
#include "runtime/ObjectProto.h"
#include "runtime/PersistentArrayMap.h"
#include "runtime/PersistentList.h"
#include "runtime/PersistentVector.h"
#include "runtime/RTValue.h"
#include "runtime/Ratio.h"
#include "runtime/String.h"
#include "runtime/Symbol.h"
#include "runtime/defines.h"
#include "runtime/word.h"

#ifdef __cplusplus
}
#endif
#endif
