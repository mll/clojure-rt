#ifndef OBJECT_PROTO_H
#define OBJECT_PROTO_H
#include <stdatomic.h>

enum objectType {
   integerType = 1,
   stringType,
   persistentListType,
   persistentVectorType,
   persistentVectorNodeType,
   doubleType,
   nilType,
   booleanType,
   symbolType,
   classType,
   deftypeType,
   concurrentHashMapType,
   keywordType,
   functionType,
   varType,
   bigIntegerType,
   ratioType,
   persistentArrayMapType,
};

typedef enum objectType objectType;


struct Object {
#ifdef REFCOUNT_NONATOMIC
  uint64_t refCount;
#endif
  volatile atomic_uint_fast64_t atomicRefCount;
  objectType type;
#ifdef USE_MEMORY_BANKS
  unsigned char bankId;
#endif
};
typedef struct Object Object; 

#endif
