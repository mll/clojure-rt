#ifndef OBJECT_PROTO_H
#define OBJECT_PROTO_H
#include <stdatomic.h>
#include "word.h"

enum objectType {
  integerType = 1,
  doubleType,               // 2 
  nilType,                  // 3 
  booleanType,              // 4
  symbolType,               // 5
  keywordType,              // 6

  stringType,               // 7
  persistentListType,       // 8
  persistentVectorType,     // 9
  persistentVectorNodeType, // 10
  concurrentHashMapType,    // 11
  functionType,             // 12
  bigIntegerType,           // 13
  ratioType,                // 14
  classType,                // 15
  persistentArrayMapType,   // 16
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
