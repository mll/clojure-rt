#ifndef OBJECT_PROTO_H
#define OBJECT_PROTO_H
#include <stdatomic.h>
#include "word.h"

enum objectType {
  integerType = 1,
  doubleType,
  nilType,
  booleanType,
  symbolType,
  keywordType,

  stringType,
  persistentListType,
  persistentVectorType,
  persistentVectorNodeType,
  concurrentHashMapType,
  functionType,
  bigIntegerType,
  ratioType,
  classType,
  persistentArrayMapType,
};

typedef enum objectType objectType;

struct Object {
#ifdef REFCOUNT_NONATOMIC
  uword_t refCount;
#endif
  _Atomic uword_t atomicRefCount;
  objectType type;
#ifdef USE_MEMORY_BANKS
  unsigned char bankId;
#endif
};
typedef struct Object Object; 

#endif
