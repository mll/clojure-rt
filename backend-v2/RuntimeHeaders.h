#ifndef RT_RUNTIME_HEADERS
#define RT_RUNTIME_HEADERS

#include <gmp.h>

// For C++ to understand C's 'restrict' keyword
#if defined(__cplusplus)
#if defined(__GNUC__) || defined(__clang__)
#define restrict __restrict__
#elif defined(_MSC_VER)
#define restrict __restrict
#else
#define restrict
#endif
#endif

#include "runtime/ObjectProto.h"
#include "runtime/RTValue.h"
#include "runtime/defines.h"
#include "runtime/word.h"

#include "runtime/BigInteger.h"
#include "runtime/Boolean.h"
#include "runtime/Class.h"
#include "runtime/ConcurrentHashMap.h"
#include "runtime/Double.h"
#include "runtime/Function.h"
#include "runtime/Integer.h"
#include "runtime/Keyword.h"
#include "runtime/Nil.h"
#include "runtime/Object.h"
#include "runtime/PersistentArrayMap.h"
#include "runtime/PersistentList.h"
#include "runtime/PersistentVector.h"
#include "runtime/PersistentVectorIterator.h"
#include "runtime/PersistentVectorNode.h"
#include "runtime/Ratio.h"
#include "runtime/String.h"
#include "runtime/Symbol.h"

#endif
